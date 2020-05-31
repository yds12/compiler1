
#include <string.h>  // remove this from here if helper functions are moved
#include <stdlib.h>  // remove this from here if helper functions are moved
#include "scoper.h"
#include "util.h"
#include "ast.h"
#include "cli.h"

// Initial size of a symbol table
#define MAX_INITIAL_SYMBOLS 10

void tryAddSymbol(Node* scopeNode, Token* token, SymbolType type);
short lookupSymbol(Node* scopeNode,Token* symToken);
void resolveScope(Node* node);
void printScopeNodes(Node* node);
short hasSymbol(Node* scopeNode, Token* symToken);
void scoperError(char* msg, int lnum, int chnum);
Node* getImmediateScope(Node* node);
SymbolTable* createSymTable(Node* scopeNode);
void addSymbol(Node* scopeNode, Symbol symbol);
void printSymTable(Node* scopeNode);

// Only the program and statement blocks have scope
int bearsScope(Node* node);

void scopeCheckerStart(FILE* file, char* filename, Node* ast) {
  scoperState = (ScoperState) {
    .file = file,
    .filename = filename
  };

  if(cli.outputType <= OUT_DEBUG)
    printf("Starting scope checking...\n");

  postorderTraverse(ast, &resolveScope);
}

void tryAddSymbol(Node* node, Token* token, SymbolType type) {
  Symbol newSym = { token, type }; 
  char exists = lookupSymbol(node, token);

  if(exists) {
    char* fmt = "Redeclaration of '%s'.";
    char msg[strlen(fmt) + token->nameSize];
    sprintf(msg, fmt, token->name);
    scoperError(msg, token->lnum, token->chnum);
  }
  Node* scopeNode = getImmediateScope(node);
  addSymbol(scopeNode, newSym);
}

SymbolTable* createSymTable(Node* scopeNode) {
  scopeNode->symTable = (SymbolTable*) malloc(sizeof(SymbolTable));
  scopeNode->symTable->nSymbols = 0;
  scopeNode->symTable->maxSize = MAX_INITIAL_SYMBOLS;
  scopeNode->symTable->symbols = (Symbol**) 
    malloc(sizeof(Symbol*) * scopeNode->symTable->maxSize);

  return scopeNode->symTable;
}

void addSymbol(Node* scopeNode, Symbol symbol) {
//printf("Adding %s to NT %d\n", symbol.token->name, scopeNode->type);
  SymbolTable* st = scopeNode->symTable;
  if(!st) st = createSymTable(scopeNode);

  if(st->nSymbols >= st->maxSize) {
    st->symbols = (Symbol**) realloc(
      st->symbols, sizeof(Symbol*) * st->maxSize * 2);

    st->maxSize *= 2;
  }

  Symbol* newSym = (Symbol*) malloc(sizeof(Symbol));
  *newSym = symbol;
  st->symbols[st->nSymbols] = newSym;
  st->nSymbols++;
}

// TODO return symbol type instead of boolean
short lookupSymbol(Node* node, Token* symToken) {
  Node* lookNode = node;

  while(1) {
//printf("Searching %s in NT %d\n", symbol->token->name, lookNode->type);
    printSymTable(lookNode);

    if(bearsScope(lookNode) && hasSymbol(lookNode, symToken)) return 1;
    if(!lookNode->parent) return 0;
    lookNode = lookNode->parent;
  }
}

short hasSymbol(Node* scopeNode, Token* symToken) {
  if(!scopeNode->symTable) return 0;  // node doesn't have a symbol table

  SymbolTable* st = scopeNode->symTable;
  if(st->nSymbols == 0) return 0;

  for(int i = 0; i < st->nSymbols; i++) {
    Symbol* tabSymbol = st->symbols[i];
    if(tabSymbol->token->nameSize != symToken->nameSize) continue;
    if(strncmp(tabSymbol->token->name, symToken->name, 
         symToken->nameSize) == 0) return 1;
  }
  return 0;
}

Node* getImmediateScope(Node* node) {
  if(bearsScope(node)) return node;
  if(!node->parent) {
    genericError("Compiler bug: AST node without scope.");
  }
  return getImmediateScope(node->parent);
}

void printScopeNodes(Node* node) {
  if(cli.outputType > OUT_DEBUG) return;

  if(bearsScope(node))
    printf("Node ID %d type %d has scope.\n", node->id, node->type);
  else
    printf("Node ID %d type %d has NO scope.\n", node->id, node->type);
}

void resolveScope(Node* node) {
  if(node->type == NTIdentifier) {
    if(!node->parent) 
      genericError("Compiler bug: AST node missing parent.");

    Node* scopeNode = node;
    Node* parent = node->parent;
    SymbolType stype;

    if(parent->type == NTExpression || parent->type == NTAssignment
       || parent->type == NTCallExpr || parent->type == NTCallSt) {
      // identifier in use  -- check if declared 
      Token* token = node->children[0]->token;
      char exists = lookupSymbol(node, token);

      // TODO function hoisting
      if(!exists) { // undeclared
        char* fmt = "Use of undeclared variable or function '%s'.";
        char msg[strlen(fmt) + token->nameSize];
        sprintf(msg, fmt, token->name);
        
        scoperError(msg, token->lnum, token->chnum);
      }
    } else { // identifier in declaration
      if(parent->type == NTFunction) { // function name
        stype = STFunction;
      } else if(parent->type == NTDeclaration) { // variable name
        Node* ppNode = parent->parent;

        if(!ppNode) 
          genericError("Compiler bug: AST node missing parent.");

        if(ppNode->type == NTProgramPart) { // global variable
          stype = STGlobal;
        } else if(ppNode->type == NTStatement) { // local variable
          stype = STLocal;
        }

      } else if(parent->type == NTArg) { // function argument
        stype = STArg;

        if(parent->parent->parent->nChildren < 3)
          genericError("Compiler bug: Function AST node missing statement.");

        scopeNode = parent->parent->parent->children[2];
      }

      if(node->nChildren < 1)
        genericError("Compiler bug: Identifier AST node without child.");

      tryAddSymbol(scopeNode, node->children[0]->token, stype);
    }
  }
}

int bearsScope(Node* node) {
  if(node->type == NTProgram) return 1;
  if(node->type == NTStatement && node->nChildren > 0) {
    for(int i = 0; i < node->nChildren; i++) {
      if(!node->children[i]) {
        genericError("Internal bug: NULL AST node child.");
      }
      if(node->children[i]->type != NTStatement) return 0;
    }
    return 1;
  }
  return 0;
}

void scoperError(char* msg, int lnum, int chnum) {
  if(cli.outputType > OUT_DEFAULT) exit(1);

  if(lnum > 0) {
    printf("\nScope " ERROR_COLOR_START "ERROR" COLOR_END ": %s\n", msg);
    printCharInFile(scoperState.file, scoperState.filename, lnum, chnum);
  } else {
    printf("\nScope " ERROR_COLOR_START "ERROR" COLOR_END 
      ": %s\n%s.\n", msg, scoperState.filename);
  }
  exit(1);
}

void printSymTable(Node* scopeNode) {
  if(cli.outputType > OUT_DEBUG) return;
  if(!scopeNode->symTable) {
    printf("NT %d: No symtable.\n", scopeNode->type); 
    return;
  }
//  printf("NT %d: symtable %p.\n", scopeNode->type, scopeNode->symTable);
  printf("NT %d: symtable has %d.", scopeNode->type, 
    scopeNode->symTable->nSymbols);
  if(scopeNode->symTable->nSymbols > 0) {
    for(int i = 0; i < scopeNode->symTable->nSymbols; i++)
      printf(" %s [T:%d]", scopeNode->symTable->symbols[i]->token->name,
        scopeNode->symTable->symbols[i]->type);
  }
  printf("\n");
}

