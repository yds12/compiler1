/*
 *
 *
 * This file contains global variables that define compilation options,
 * and functions to parse command line arguments.
 *
 */

#ifndef CLI_H
#define CLI_H

// Version of the compiler.
#define VERSION "0.0.6"

// Compiler output type
#define OUT_DEBUG 0           // all output
#define OUT_VERBOSE 1         // all non-debug output
#define OUT_DEFAULT 2         // default output
#define OUT_SILENT 3          // no output

#define OUT_GRAPHVIZ 4        // outputs parse tree in Graphviz format

// Global compiler options variables
int CLI_OUTPUT_TYPE;

struct stCli {
  short outputType;
};

struct stCli cli;

int parseCLArgs(int argc, char ** argv);

#endif
