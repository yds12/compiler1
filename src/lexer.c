/*
 *
 * Main file for the lexer of the compiler. The specification of the
 * valid language tokens is at docs/lexicon.txt.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "lexer.h"

/*
 * Reads the next character of the source file and manages the related
 * lexer state variables.
 *
 * returns: the character read or EOF.
 *
 */
char lexerGetChar();

/*
 * Adds a new token to the list of processed tokens. 
 *
 * size: the amount of characters (bytes) in this token.
 * type: type of token (identifier, integer literal, AND operator, etc.)
 * lnum: line number where this token was found.
 * chnum: position/column number where the token was found in the line.
 *
 */
void addToken(int size, TokenType type, int lnum, int chnum);

/*
 * Processes identifiers and keywords.
 *
 */
void eatIDKW();

/*
 * Processes integers and float number literals.
 *
 */
void eatNumber();

/*
 * Processes comments and the division operator. 
 *
 */
void eatSlash(); 

/*
 * Processes tokens constituted of a single character, like (, ), {, }, etc.
 *
 *
 * Note: Need to improve string eating. Only valid ASCII or UTF-8 
 * strings should be allowed.
 *
 */
void eatDQuote();

/*
 * Processes tokens constituted of a single character, like (, ), {, }, etc.
 *
 */
void eatSingleSymb();

/*
 * Processes tokens constituted of two repeated characters,
 * such as ==, ++, --.
 *
 */
void eatDoubleSymb();

// Size of the buffer used to read characters
#define BUFFER_SIZE 100000

// Maximum number of tokens.
#define MAX_TOKENS 1000

// To show debug messages:
#define DEBUG


void lexerStart(FILE* sourcefile, char* sourcefilename) {

// Prints the source file
#ifdef DEBUG
  printFile(sourcefile);
#endif

  filename = sourcefilename;

  // The list of tokens in the source file
  tokens = (Token*) malloc(MAX_TOKENS * sizeof(Token));

  // Buffer to read the chars, is reset at the end of each token
  char charBuffer[BUFFER_SIZE];
  n_tokens = 0; // number of tokens processed so far

  // Holds global state variables of the lexer
  lexerState = (LexerState) {
    .buffer = charBuffer,
    .file = sourcefile,
    .lastChar = '\0',
    .lnum = 1,
    .chnum = 1
  };

  // starts with the first character
  if(!feof(sourcefile)) {
    charBuffer[0] = lexerGetChar();
  }

  while(!feof(sourcefile)) {
    char ch = charBuffer[0];

    // depending on the character read, call a method to process next token(s)
    // All functions should finish with the next character in buffer[0],
    // and lexerState.chnum as the position (column) of buffer[0]
    if(ch == '/') eatSlash();
    else if(ch == '"') eatDQuote();
    else if(isSingleCharOp(ch)) eatSingleSymb();
    else if(belongsToDoubleOp(ch)) eatDoubleSymb();
    else if(isAlpha(ch) || ch == '_') eatIDKW();
    else if(isNum(ch)) eatNumber();
    else if(isWhitespace(ch)) charBuffer[0] = lexerGetChar();
  }

// Prints info about the tokens processed
#ifdef DEBUG
  for(int i = 0; i < n_tokens; i++) {
    printf("\n\n");
    printf("tt_%d @%d,%d, len: %d, ||%s||\n", tokens[i].type, 
      tokens[i].lnum, tokens[i].chnum, tokens[i].nameSize, tokens[i].name);

    printTokenInFile(sourcefile, tokens[i]);
  }
  printf("Total tokens: %d\n", n_tokens);
#endif

  fclose(sourcefile);
}

void eatIDKW()
{
  int bufpos = 1;
  char* buffer = lexerState.buffer;
  int lnum = lexerState.lnum;
  int chnum = lexerState.chnum;
  char ch = lexerGetChar();
  TokenType type = TTId;

  while(isNum(ch) || isAlpha(ch) || ch == '_') { // Still ID or keyword
    buffer[bufpos] = ch;
    ch = lexerGetChar();
    bufpos++;
  }

  switch(bufpos) {
    case 2:
      if(strncmp("if", buffer, bufpos) == 0) type = TTIf;
      else if(strncmp("or", buffer, bufpos) == 0) type = TTOr;
      else if(strncmp("fn", buffer, bufpos) == 0) type = TTFunc;
      break;
    case 3:
      if(strncmp("and", buffer, bufpos) == 0) type = TTAnd;
      else if(strncmp("for", buffer, bufpos) == 0) type = TTFor;
      else if(strncmp("int", buffer, bufpos) == 0) type = TTInt;
      else if(strncmp("not", buffer, bufpos) == 0) type = TTNot;
      break;
    case 4:
      if(strncmp("bool", buffer, bufpos) == 0) type = TTBool;
      else if(strncmp("else", buffer, bufpos) == 0) type = TTElse;
      else if(strncmp("next", buffer, bufpos) == 0) type = TTNext;
      break;
    case 5:
      if(strncmp("break", buffer, bufpos) == 0) type = TTBreak;
      else if(strncmp("float", buffer, bufpos) == 0) type = TTFloat;
      else if(strncmp("while", buffer, bufpos) == 0) type = TTWhile;
      break;
    case 6:
      if(strncmp("string", buffer, bufpos) == 0) type = TTString;
      break;
  }

  // Add the ID/keyword token
  addToken(bufpos, type, lnum, chnum);
  buffer[0] = lexerState.lastChar;
  return;
}

void eatNumber()
{
  int bufpos = 1;
  char* buffer = lexerState.buffer;
  int lnum = lexerState.lnum;
  int chnum = lexerState.chnum;
  char ch = lexerGetChar();
  TokenType type = TTLitInt;

  while(isNum(ch)) {
    buffer[bufpos] = ch;
    ch = lexerGetChar();
    bufpos++;
  }

  if(ch == '.') { // float, read the rest of the number
    buffer[bufpos] = ch;
    ch = lexerGetChar();
    bufpos++;

    if(isNum(ch)) {
      type = TTLitFloat;

      while(isNum(ch)) {
        buffer[bufpos] = ch;
        ch = lexerGetChar();
        bufpos++;
      }

    } else {
      error("Invalid number.");
    }
  }

  addToken(bufpos, type, lnum, chnum);
  buffer[0] = lexerState.lastChar;
  return;
}

void eatDoubleSymb()
{
  int bufpos = 1;
  char* buffer = lexerState.buffer;
  int lnum = lexerState.lnum;
  int chnum = lexerState.chnum;

  char ch = lexerGetChar();
  TokenType type = TTUnknown;

  if(ch == buffer[0]) { // double symbol
    switch(buffer[0]) {
      case '=': type = TTEq;
        break;
      case '+': type = TTIncr;
        break;
      case '-': type = TTDecr;
        break;
    }

    buffer[bufpos] = ch;
    addToken(2, type, lnum, chnum);
    buffer[0] = lexerGetChar();
  } else { // single symbol
    switch(buffer[0]) {
      case '=': type = TTAssign;
        break;
      case '+': type = TTPlus;
        break;
      case '-': type = TTMinus;
        break;
    }

    addToken(1, type, lnum, chnum);
    buffer[0] = ch;
  }
  return;
}

void eatSingleSymb()
{
  char* buffer = lexerState.buffer;
  int lnum = lexerState.lnum;
  int chnum = lexerState.chnum;
  int type = TTUnknown;

  switch(buffer[0]) {
    case '(': type = TTLPar;
      break;
    case ')': type = TTRPar;
      break;
    case '{': type = TTLBrace;
      break;
    case '}': type = TTRBrace;
      break;
    case ';': type = TTSemi;
      break;
    case '*': type = TTMult;
      break;
    case '%': type = TTMod;
      break;
    case ':': type = TTColon;
      break;
  }

  addToken(1, type, lnum, chnum);
  buffer[0] = lexerGetChar();
  return;
}

void eatDQuote()
{
  int bufpos = 1;
  char* buffer = lexerState.buffer;
  int lnum = lexerState.lnum;
  int chnum = lexerState.chnum;
  char ch = lexerGetChar();

  while(ch != '\n' && ch != '"') { // read string till end
    buffer[bufpos] = ch;
    ch = lexerGetChar();
    bufpos++;
  }

  if(ch == '\n') error("Line break in the middle of string.");

  // Here ch == '"'
  buffer[bufpos] = ch;
  addToken(bufpos + 1, TTLitString, lnum, chnum);
  buffer[0] = lexerGetChar();
  return;
}

void eatSlash()
{
  char* buffer = lexerState.buffer;
  int lnum = lexerState.lnum;
  int chnum = lexerState.chnum;
  char ch = lexerGetChar();

  if(ch == '/') { // it was a comment
    lexerGetChar();

    // discard until newline
    while(lexerState.lastChar != '\n') lexerGetChar();
    buffer[0] = lexerGetChar();
  } else { // not a comment, thus division
    addToken(1, TTDiv, lnum, chnum); // adds division op.
    buffer[0] = ch;
  }
  return;
}

void addToken(int size, TokenType type, int lnum, int chnum) {
  char* tokenName = (char*) malloc((size + 1) * sizeof(char));
  Token token = { tokenName, size, type, lnum, chnum };
  strncpy(tokenName, lexerState.buffer, size);
  token.name[size] = '\0';

  if(n_tokens >= MAX_TOKENS) {
    error("Too many tokens.");
  }
  tokens[n_tokens] = token;
  n_tokens++;
}

int isWhitespace(char character) {
  if(character == ' ' || character == '\n' || character == '\t') return 1;
  return 0;
}

int isAlpha(char character) {
  if((character >= 'a' && character <= 'z') ||
    (character >= 'A' && character <= 'Z')) {
    return 1;
  }
  return 0;
}

int isNum(char character) {
  if(character >= '0' && character <= '9') return 1;
  return 0;
}

int belongsToDoubleOp(char character) {
  if(character == '=' || character == '+' || character == '-') return 1;
  return 0;
}

int isSingleCharOp(char ch) {
  if(ch == '(' || ch == ')' || ch == '{' || ch == '}' ||
     ch == ';' || ch == '*' || ch == '%' || ch == ':')
    return 1;
  return 0;
}

char lexerGetChar() {
  if(!feof(lexerState.file)) {
    char ch = fgetc(lexerState.file);
    lexerState.lastChar = ch;
    lexerState.prevLnum = lexerState.lnum;
    lexerState.prevChnum = lexerState.chnum;

    if(ch == '\n') {
      lexerState.lnum++;
      lexerState.chnum = 0;
    } else {
      lexerState.chnum++;
    }
    return ch;
  }
  else return EOF;
}

void printTokenInFile(FILE* file, Token token) {
  rewind(file);

  int BUFF_SIZE = 80;
  char buff[BUFF_SIZE + 1];
  char buff_mark[BUFF_SIZE + 1];

  for(int i = 0; i <= BUFF_SIZE; i++) {
    buff[i] = '\0';
    buff_mark[i] = '\0';
  }

  int lnum = 1;
  int chnum = 1;
  while(!feof(file)) {
    char ch = fgetc(file);

    if(lnum == token.lnum) {
      if(chnum < BUFF_SIZE) buff[chnum - 1] = ch;
    } else if(lnum > token.lnum) break;

    if(ch == '\n') {
      lnum++;
      chnum = 1;
    } else chnum++;
  }

  for(int i = 0; i < BUFF_SIZE; i++) {
    if(i + 1 < token.chnum || i + 1 >= token.chnum + token.nameSize) {
      buff_mark[i] = ' '; 
    }
    else buff_mark[i] = '^';
  }

  printf("\nToken '%s':\n", token.name);
  printf("%s:%d:%d:\n\n", filename, token.lnum, token.chnum);
  printf("%s", buff);
  printf("%s\n", buff_mark);
}

void printFile(FILE* file) {
  rewind(file);
  char ch = fgetc(file);

  while(!feof(file)) {
    printf("%c", ch);
    ch = fgetc(file);
  }
  printf("\n");
  rewind(file);
}

void error(char* msg) {
  printf("ERROR: %s\n%s: line: %d, column: %d.\n", msg, filename, 
    lexerState.lnum, lexerState.chnum);
  exit(1);
}

