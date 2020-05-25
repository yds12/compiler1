#include <stdio.h>
#include <string.h>
#include "lexer.h"
#include "parser.h"

#define DEBUG

void shift();
int reduce();
int reduceSemi();
int reduceExpression();
int reduceIdentifier();
void reduceRoot();
void printStack();

// Replaces the current stack top with a parent node of specified type
void singleParent(NodeType type);

// Checks whether a certain type can come before a statement
int canPrecedeStatement(Node* node);

void parserStart() {
  parserState = (ParserState) { 
    .file = lexerState.file, 
    .filename = lexerState.filename,
    .nextToken = 0
  };

  initializeStack();

  while(parserState.nextToken < lexerState.nTokens) {
    shift();
    int success = 0;

    do {
      success = reduce(); 

#ifdef DEBUG
    printf("After reduce. ");
    printStack();
#endif

    } while(success);
  }
}

void shift() {
  Token* token = &lexerState.tokens[parserState.nextToken];
  parserState.nextToken++;

  Node node = { 
    .type = NTTerminal, 
    .token = token, 
    .children = NULL,
    .nChildren = 0
  };
  createAndPush(node, 0);

#ifdef DEBUG
    printf("After shift.  ");
    printStack();
#endif

}

int reduce() {
  Node* curNode = pStack.nodes[pStack.pointer];
  Node* prevNode = NULL;
  if(pStack.pointer >= 1) prevNode = pStack.nodes[pStack.pointer - 1];

  int reduced = 0;

#ifdef DEBUG
    printf("Entering reduce with type %d\n", curNode->type);
#endif

  if(curNode->type == NTProgramPart) { // DONE
    if(lookAhead().type == TTEof) {
      reduceRoot();
      reduced = 1;
    }
  } else if(curNode->type == NTStatement) { // UNFINISHED

    if(!prevNode || prevNode->type == NTProgramPart) {
      // we have a finished statement and the previous statement is already
      // reduced.
      singleParent(NTProgramPart);
      reduced = 1;
    } else if(!canPrecedeStatement(prevNode)) {
      // build error string
      char* format = "Unexpected %s before statement.";
      int len = strlen(format) + MAX_NODE_NAME;
      char str[len];
      strReplaceNodeName(str, format, prevNode); 

      Node* problematic = astLastLeaf(prevNode);
      parsError(str, problematic->token->lnum, problematic->token->chnum);
    }

  } else if(curNode->type == NTFunction) { // DONE
    singleParent(NTProgramPart);
    reduced = 1;
  } else if(isSubStatement(curNode->type)) { // DONE
    // Substatements: NTIfSt, NTNoop, NTNextSt, NTBreakSt, NTWhileSt,
    // NTMatchSt, NTLoopSt. 
    singleParent(NTStatement);
    reduced = 1;
  } else if(curNode->type == NTExpression) { // UNFINISHED 
    reduced = reduceExpression();
  } else if(curNode->type == NTTerm) { // UNFINISHED
    singleParent(NTExpression);
    reduced = 1;
  } else if(curNode->type == NTLiteral) { // DONE
    singleParent(NTTerm);
    reduced = 1;
  } else if(curNode->type == NTBinaryOp) { // UNFINISHED
  } else if(curNode->type == NTIdentifier) { // UNFINISHED
    reduced = reduceIdentifier();
  } else if(curNode->type == NTTerminal) { // UNFINISHED
    TokenType ttype = curNode->token->type;

    if(isBinaryOp(ttype)) {
      singleParent(NTBinaryOp);
      reduced = 1;
    } else if(isLiteral(ttype)) {
      singleParent(NTLiteral);
      reduced = 1;
    } else if(isType(ttype)) {
      singleParent(NTType);
      reduced = 1;
    } else if(ttype == TTId) {
      singleParent(NTIdentifier);
      reduced = 1;
    } else if(ttype == TTBreak) { // wait next shift
    } else if(ttype == TTNext) { // wait next shift
    } else if(ttype == TTSemi) {
      reduced = reduceSemi();
    }
  }

  return reduced;
}

int reduceIdentifier() {
  int reduced = 0;
  Node* curNode = pStack.nodes[pStack.pointer];
  Node* prevNode = NULL;
  if(pStack.pointer >= 1) prevNode = pStack.nodes[pStack.pointer - 1];
  TokenType ttype = lookAhead().type;

  if(ttype == TTId || ttype == TTLPar || ttype == TTNot || isLiteral(ttype)) {
    // is function ID (will be reduced later)
  } else { // is variable
    if(prevNode && prevNode->type == NTTerminal 
       && isType(prevNode->token->type)) // in declaration
    {
      Node* prevPrevNode = NULL;
      if(pStack.pointer >= 2) prevPrevNode = pStack.nodes[pStack.pointer - 2];

      if(!prevPrevNode || prevPrevNode->type == NTProgramPart ||
         prevPrevNode->type == NTStatement)
      {
        // variable declaration, will reduce later
      } else { // is parameter
        stackPop(2);
        Node node = { 
          .type = NTParam, 
          .token = NULL, 
          .children = NULL 
        };
        Node* nodePtr = createAndPush(node, 2);
        nodePtr->children[0] = prevNode;
        nodePtr->children[1] = curNode;
        reduced = 1;
      }
    } else { // term
      singleParent(NTTerm);
      reduced = 1;
    }
  }

  return reduced;
}

int reduceExpression() {
  int reduced = 0;
  Node* curNode = pStack.nodes[pStack.pointer];
  Node* prevNode = NULL;
  if(pStack.pointer >= 1) prevNode = pStack.nodes[pStack.pointer - 1];

  if(prevNode && prevNode->type == NTBinaryOp) {
    Node* prevPrevNode = NULL;
    if(pStack.pointer >= 2) prevPrevNode = pStack.nodes[pStack.pointer - 2];

    TokenType nextTType = lookAhead().type;

    if(nextTType == TTRPar) {
      if(prevPrevNode->type != NTExpression) {
        // If we see a OP EXPR sequence, there must be an EXPR before that 
        char* format = "Expected expression before operator, found %s.";
        int len = strlen(format) + MAX_NODE_NAME;
        char str[len];
        strReplaceNodeName(str, format, prevPrevNode); 

        Node* problematic = astLastLeaf(prevPrevNode);
        parsError(str, problematic->token->lnum, problematic->token->chnum);
      } else { // EXPR OP EXPR ) sequence, EXPR OP EXPR => EXPR
        stackPop(3);
        Node node = { 
          .type = NTExpression, 
          .token = NULL, 
          .children = NULL // set in createAndPush()
        };
        Node* nodePtr = createAndPush(node, 3);
        nodePtr->children[0] = prevPrevNode;
        nodePtr->children[1] = prevNode;
        nodePtr->children[2] = curNode;
        reduced = 1;
      }
    }
  }

  return reduced;
}

int reduceSemi() {
  int reduced = 0;
  Node* curNode = pStack.nodes[pStack.pointer];
  Node* prevNode = NULL;
  if(pStack.pointer >= 1) prevNode = pStack.nodes[pStack.pointer - 1];

  if(!prevNode || prevNode->type == NTProgramPart) {
    singleParent(NTNoop);
    reduced = 1;
  } else if(prevNode->type == NTTerminal) {
    TokenType ttype = prevNode->token->type;

    if(ttype == TTBreak || ttype == TTNext) {
      NodeType nType = NTBreakSt;
      if(ttype == TTNext) nType = NTNextSt;

      stackPop(2);
      Node node = { 
        .type = nType, 
        .token = NULL, 
        .children = NULL 
      };
      Node* nodePtr = createAndPush(node, 2);
      nodePtr->children[0] = prevNode;
      nodePtr->children[1] = curNode;
      reduced = 1;
    }
  }

  return reduced;
}

void reduceRoot() {
  for(int i = 0; i <= pStack.pointer; i++) {
    if(pStack.nodes[i]->type != NTProgramPart) {
      // build error string
      char* format = "Unexpected %s at program root level.";
      int len = strlen(format) + MAX_NODE_NAME;
      char str[len];
      strReplaceNodeName(str, format, pStack.nodes[i]); 

      Node* problematic = astFirstLeaf(pStack.nodes[i]);
      parsError(str, problematic->token->lnum, problematic->token->chnum);
    }
  }

  Node node = { 
    .type = NTProgram, 
    .token = NULL, 
    .children = NULL 
  };
  Node* nodePtr = newNode(node);
  allocChildren(nodePtr, pStack.pointer + 1);

  for(int i = 0; i <= pStack.pointer; i++) {
    nodePtr->children[i] = pStack.nodes[i];
  }

  stackPop(pStack.pointer + 1);
  stackPush(nodePtr);
}

void singleParent(NodeType type) {
  Node* curNode = pStack.nodes[pStack.pointer];
  stackPop(1);

  Node node = { 
    .type = type, 
    .token = NULL, 
    .children = NULL 
  };
  Node* nodePtr = createAndPush(node, 1);
  nodePtr->children[0] = curNode;
}

void printStack() {
  printf("Stack: ");
  for(int i = 0; i <= pStack.pointer; i++) {
    Node* node = pStack.nodes[i];
    printf(" %d", node->type);
  }
  printf(".\n");
}

int canPrecedeStatement(Node* node) {
  NodeType type = node->type;

  if(type == NTProgramPart || type == NTStatement) return 1;
  if(type == NTTerminal) {
    TokenType ttype = node->token->type;
    if(ttype == TTColon || ttype == TTComma || ttype == TTElse ||
       ttype == TTArrow || ttype == TTLBrace)
      return 1;
  }
  return 0;
}

