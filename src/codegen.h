
#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdio.h>
#include "datast.h"

#define N_GPR 7

typedef struct stCodegenState{
  FILE* file;
  char* filename;
  char* busyRegisters;  // which registers are free
  char nGPR;  // how many general purpose registers (GPR)
  char** nameGPR;  // names of the GPRs
} CodegenState;

typedef enum enInstructionType {
  INS_MOV,
  INS_ADD,
  INS_SUB
} InstructionType;

CodegenState codegenState;

void codegenStart(FILE* file, char* filename, Node* ast);

#endif

