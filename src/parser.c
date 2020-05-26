#include <stdio.h>
#include <string.h>
#include "lexer.h"
#include "parser.h"

#define DEBUG

void shift();
int reduce();
int reduceSemi();
int reduceExpression();
int reduceRPar();
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
  } else if(curNode->type == NTDeclaration) { // DONE
    if(!prevNode || prevNode->type == NTStatement
       || prevNode->type == NTProgramPart) { // independent declaration
      singleParent(NTProgramPart);
      reduced = 1;
    } else { // declaration part of FOR statement -- do nothing
    }
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
    } else if(ttype == TTRPar) {
      reduced = reduceRPar();
    } else if(ttype == TTSemi) {
      reduced = reduceSemi();
    }
  }

  return reduced;
}

int reduceRPar() {
  int reduced = 0;
  Node* curNode = pStack.nodes[pStack.pointer];
  Node* prevNode = NULL;
  Node* prevPrevNode = NULL;
  if(pStack.pointer >= 1) prevNode = pStack.nodes[pStack.pointer - 1];
  if(pStack.pointer >= 2) prevPrevNode = pStack.nodes[pStack.pointer - 2];

  if(!prevNode) {
    Node* problematic = astLastLeaf(curNode);
    parsError("Program beginning with ')'.", 
      problematic->token->lnum, problematic->token->chnum);
  }
  if(prevNode->type == NTProgramPart || prevNode->type == NTStatement) {
    char* format = "Unexpected ')' after %s.";
    int len = strlen(format) + MAX_NODE_NAME;
    char str[len];
    strReplaceNodeName(str, format, prevNode); 

    Node* problematic = astLastLeaf(prevNode);
    parsError(str, problematic->token->lnum, problematic->token->chnum);
  }

  if(prevNode->type == NTExpression) {
    if(prevPrevNode && prevPrevNode->type == NTTerminal &&
       prevPrevNode->token->type == TTLPar) {
      // (EXPR) -- reduce
      stackPop(3);
      Node node = { 
        .type = NTExpression, 
        .token = NULL, 
        .children = NULL
      };
      Node* nodePtr = createAndPush(node, 1);
      nodePtr->children[0] = prevNode; // parentheses are ignored
      reduced = 1;
    }
  } else if(prevNode->type == NTCallParam) {
    // ID(PARAM,...,PARAM) -- function call
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
    if(prevNode && prevNode->type == NTType) // in declaration
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
  Node* prevPrevNode = NULL;
  if(pStack.pointer >= 1) prevNode = pStack.nodes[pStack.pointer - 1];
  if(pStack.pointer >= 2) prevPrevNode = pStack.nodes[pStack.pointer - 2];

  Token laToken = lookAhead();
  TokenType laType = laToken.type;

  if(!prevNode) { // error: starting program with expression
    Node* problematic = astLastLeaf(curNode);
    parsError("Program beginning with expression.", 
      problematic->token->lnum, problematic->token->chnum);
  }
  else if(prevNode->type == NTProgramPart || prevNode->type == NTStatement) {
    // Error: expression after complete statement
    char* format = "Unexpected expression after %s.";
    int len = strlen(format) + MAX_NODE_NAME;
    char str[len];
    strReplaceNodeName(str, format, prevNode); 

    Node* problematic = astLastLeaf(prevNode);
    parsError(str, problematic->token->lnum, problematic->token->chnum);
  }

  if(isExprTerminator(laType)) {
    if(prevNode->type == NTBinaryOp) {
      if(prevPrevNode && prevPrevNode->type != NTExpression) {
        // If we see a OP EXPR sequence, there must be an EXPR before that 
        char* format = "Expected expression before operator, found %s.";
        int len = strlen(format) + MAX_NODE_NAME;
        char str[len];
        strReplaceNodeName(str, format, prevPrevNode); 

        Node* problematic = astLastLeaf(prevPrevNode);
        parsError(str, problematic->token->lnum, problematic->token->chnum);
      } else { // EXPR OP EXPR TERMINATOR sequence, EXPR OP EXPR => EXPR

#ifdef DEBUG
printf("REDUCED EXPR: %s EXPR %s\n", prevNode->children[0]->token->name,
laToken.name);
#endif

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
  } else if(isBinaryOp(laType)) {
    if(prevNode->type == NTBinaryOp) {
      if(prevPrevNode && prevPrevNode->type != NTExpression) {
        // If we see a OP EXPR sequence, there must be an EXPR before that 
        char* format = "Expected expression before operator, found %s.";
        int len = strlen(format) + MAX_NODE_NAME;
        char str[len];
        strReplaceNodeName(str, format, prevPrevNode); 

        Node* problematic = astLastLeaf(prevPrevNode);
        parsError(str, problematic->token->lnum, problematic->token->chnum);
      } else if(precedence(laType) >= 
                precedence(prevNode->children[0]->token->type)){ 

#ifdef DEBUG
printf("REDUCED EXPR: %s EXPR %s\n", prevNode->children[0]->token->name,
laToken.name);
printf("PRECEDENCES: %d %d\n", precedence(prevNode->children[0]->token->type),
precedence(laType));
#endif

        // EXPR OP EXPR OP sequence, reduce if the previous has precedence
        // (<= value for precedence()), otherwise do nothing
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
  } else {
    // error: unexpected token after expression
    char* format = "Unexpected token '%s' after expression.";
    char str[strlen(format) + laToken.nameSize + 5];
    sprintf(str, format, laToken.name);
    parsError(str, laToken.lnum, laToken.chnum);
  }

  return reduced;
}

int reduceSemi() {
  int reduced = 0;
  Node* curNode = pStack.nodes[pStack.pointer];
  Node* prevNode = NULL;
  Node* prevPrevNode = NULL;
  if(pStack.pointer >= 1) prevNode = pStack.nodes[pStack.pointer - 1];
  if(pStack.pointer >= 2) prevPrevNode = pStack.nodes[pStack.pointer - 2];

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
  } else if(prevNode->type == NTExpression) {
    if(prevPrevNode && prevPrevNode->type == NTTerminal &&
       prevPrevNode->token->type == TTAssign) { 
      // assignment or declaration with assignment
      Node* prev3 = NULL;
      Node* prev4 = NULL;
      if(pStack.pointer >= 1) prev3 = pStack.nodes[pStack.pointer - 3];
      if(pStack.pointer >= 2) prev4 = pStack.nodes[pStack.pointer - 4];

      if(prev3 && prev3->type == NTIdentifier) {
        if(!prev4) { // error
          char* format = "Assignment to undeclared variable '%s'.";
          int len = strlen(format) + prev3->children[0]->token->nameSize;
          char str[len];
          sprintf(str, format, prev3->children[0]->token->name);

          Node* problematic = astFirstLeaf(prev3);
          parsError(str, problematic->token->lnum, problematic->token->chnum);
        } else if(prev4->type == NTProgramPart || prev4->type == NTStatement) {
          // ID = EXPR ;  -- assignment
          stackPop(4);
          Node node = { 
            .type = NTAssignment, 
            .token = NULL, 
            .children = NULL // set in createAndPush()
          };
          Node* nodePtr = createAndPush(node, 2);
          nodePtr->children[0] = prev3; // ID
          nodePtr->children[1] = prevNode; // EXPR
          reduced = 1;
        } else if(prev4->type == NTType) { // declaration w/ assignment
          // TYPE ID = EXPR ;  -- declaration with assignment
          stackPop(5);
          Node node = { 
            .type = NTDeclaration, 
            .token = NULL, 
            .children = NULL // set in createAndPush()
          };
          Node* nodePtr = createAndPush(node, 3);
          nodePtr->children[0] = prev4; // TYPE
          nodePtr->children[1] = prev3; // ID
          nodePtr->children[2] = prevNode; // EXPR
          reduced = 1;
        }
      } else { // error
        char* format = "Assignment to %s.";
        int len = strlen(format) + MAX_NODE_NAME;
        char str[len];
        strReplaceNodeName(str, format, prev3); 

        Node* problematic = astFirstLeaf(prev3);
        parsError(str, problematic->token->lnum, problematic->token->chnum);
      }
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

