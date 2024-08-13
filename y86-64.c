#include <stdio.h>
#include <stdlib.h>

#include "utils.h"

const int MAX_MEM_SIZE  = (1 << 13);

void getRegisters(int *rA, int *rB, wordType valP) {
  int registerByte = getByteFromMemory(valP + 1);
  *rA = registerByte >> 4;
  *rB = registerByte % 16;
}

bool checkOFlag(wordType valA, wordType valB, wordType valE, int ifun) {
  bool OFlag = FALSE;
  if (ifun == ADD) {
    if ((valA > 0) && (valB > 0) && (valE < 0))
      OFlag = TRUE;
  }
  if (ifun == SUB) {
    if ((valA < 0) && (valB < 0) && (valE > 0))
      OFlag = TRUE;
  }
  return OFlag;
}

void fetchStage(int *icode, int *ifun, int *rA, int *rB, wordType *valC, wordType *valP) {
  *valP = getPC();
  //printf("PC is: %04lx\n", *valP);
  int opInstruction = getByteFromMemory(*valP);
  *icode = opInstruction >> 4;
  *ifun = opInstruction % 16;
  if (*icode == NOP || *icode == RET) {
    *valP += 1;
  }
  if (*icode == HALT) {
    setStatus(STAT_HLT);
    *valP += 1;
    //printf("Executing HALT: icode = %x\n", *icode);
  }
  if (*icode == RRMOVQ || *icode == OPQ || *icode == POPQ || *icode == PUSHQ) {
    getRegisters(rA,rB,*valP);
    *valP += 2;
  }
  if (*icode == IRMOVQ || *icode == MRMOVQ || *icode == RMMOVQ) {
    getRegisters(rA,rB,*valP);
    *valC = getWordFromMemory(*valP + 2);
    *valP += 10;
  }
  if (*icode == JXX || *icode == CALL) {
    *valC = getWordFromMemory(*valP + 1);
    *valP += 9;
  }
}

void decodeStage(int icode, int rA, int rB, wordType *valA, wordType *valB) {
  if (icode == RRMOVQ) {
    *valA = getRegister(rA);
  }
  if (icode == RMMOVQ || icode == OPQ) {
    *valA = getRegister(rA);
    *valB = getRegister(rB);
  }
  if (icode == MRMOVQ) {
    *valB = getRegister(rB);
  }
  if (icode == PUSHQ) {
    *valA = getRegister(rA);
    *valB = getRegister(RSP);
  }
  if (icode == POPQ || icode == RET) {
    *valA = getRegister(RSP);
    *valB = getRegister(RSP);
  }
  if (icode == CALL) {
    *valB = getRegister(RSP);
  }
}

void executeStage(int icode, int ifun, wordType valA, wordType valB, wordType valC, wordType *valE, bool *Cnd) {
  if (icode == RRMOVQ) {
    *valE = valA;
  }
  if (icode == IRMOVQ) {
    *valE = valC;
  }
  if (icode == RMMOVQ || icode == MRMOVQ) {
    *valE = valB + valC;
  }
  if (icode == OPQ) {
  bool zFlag = FALSE;
  bool sFlag = FALSE;
  bool oFlag = FALSE;
    if (ifun == ADD)
      *valE = valB + valA;
    if (ifun == SUB)
      *valE = valB - valA;
    if (ifun == AND)
      *valE = valB & valA;
    if (ifun == XOR)
      *valE = valB ^ valA;
    if (*valE == 0)
      zFlag = TRUE;
    if (*valE < 0)
      sFlag = TRUE;
    if (checkOFlag(valA, valB, *valE, ifun)) {
      oFlag = TRUE;
    }
  setFlags(sFlag, zFlag, oFlag);
  }
  if (icode == PUSHQ || icode == CALL) {
    *valE = valB - 8;
  }
  if (icode == POPQ || icode == RET) {
    *valE = valB + 8;
  }
  if (icode == JXX) {
    *Cnd = Cond(ifun);
  }
  /*if (icode == CMOVXX) {
    *Cnd = Cond(ifun);
    *valE = valA;
  }*/
}

void memoryStage(int icode, wordType valA, wordType valP, wordType valE, wordType *valM) {
  if (icode == RMMOVQ || icode == PUSHQ) {
    setWordInMemory(valE, valA);
  }
  if (icode == MRMOVQ) {
    *valM = getWordFromMemory(valE);
  }
  if (icode == POPQ || icode == RET) {
    *valM = getWordFromMemory(valA);
  }
  if (icode == CALL) {
    setWordInMemory(valE, valP);
  }
}

void writebackStage(int icode, int rA, int rB, wordType valE, wordType valM) {
  if (icode == RRMOVQ || icode == IRMOVQ || icode == OPQ) {
    setRegister(rB, valE);
  }
  if (icode == MRMOVQ) {
    setRegister(rA, valM);
  }
  if (icode == PUSHQ || icode == CALL || icode == RET) {
    setRegister(RSP, valE);
  }
  if (icode == POPQ) {
    setRegister(RSP, valE);
    setRegister(rA, valM);
  }
}

void pcUpdateStage(int icode, wordType valC, wordType valP, bool Cnd, wordType valM) {
  if (icode == CALL) {
    setPC(valC);
  } else if (icode == RET) {
    setPC(valM);
  } else if (Cnd) {
    setPC(valC);
  } else {
    setPC(valP);
  }
}

void stepMachine(int stepMode) {
  /* FETCH STAGE */
  int icode = 0, ifun = 0;
  int rA = 0, rB = 0;
  wordType valC = 0;
  wordType valP = 0;
 
  /* DECODE STAGE */
  wordType valA = 0;
  wordType valB = 0;

  /* EXECUTE STAGE */
  wordType valE = 0;
  bool Cnd = 0;

  /* MEMORY STAGE */
  wordType valM = 0;

  fetchStage(&icode, &ifun, &rA, &rB, &valC, &valP);
  applyStageStepMode(stepMode, "Fetch", icode, ifun, rA, rB, valC, valP, valA, valB, valE, Cnd, valM);

  decodeStage(icode, rA, rB, &valA, &valB);
  applyStageStepMode(stepMode, "Decode", icode, ifun, rA, rB, valC, valP, valA, valB, valE, Cnd, valM);
  
  executeStage(icode, ifun, valA, valB, valC, &valE, &Cnd);
  applyStageStepMode(stepMode, "Execute", icode, ifun, rA, rB, valC, valP, valA, valB, valE, Cnd, valM);
  
  memoryStage(icode, valA, valP, valE, &valM);
  applyStageStepMode(stepMode, "Memory", icode, ifun, rA, rB, valC, valP, valA, valB, valE, Cnd, valM);
  
  writebackStage(icode, rA, rB, valE, valM);
  applyStageStepMode(stepMode, "Writeback", icode, ifun, rA, rB, valC, valP, valA, valB, valE, Cnd, valM);
  
  pcUpdateStage(icode, valC, valP, Cnd, valM);
  applyStageStepMode(stepMode, "PC update", icode, ifun, rA, rB, valC, valP, valA, valB, valE, Cnd, valM);

  incrementCycleCounter();
}

/** 
 * main
 * */
int main(int argc, char **argv) {
  int stepMode = 0;
  FILE *input = parseCommandLine(argc, argv, &stepMode);

  initializeMemory(MAX_MEM_SIZE);
  initializeRegisters();
  loadMemory(input);

  applyStepMode(stepMode);
  while (getStatus() != STAT_HLT) {
    stepMachine(stepMode);
    applyStepMode(stepMode);
#ifdef DEBUG
    printMachineState();
    printf("\n");
#endif
  }
  printMachineState();
  return 0;
}
