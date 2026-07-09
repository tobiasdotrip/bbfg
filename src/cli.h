#ifndef BBFG_CLI_H
#define BBFG_CLI_H

#include <stdio.h>

typedef enum
{
  BBFG_COMMAND_OPEN_REPO,
  BBFG_COMMAND_HEAD_COMMIT,
  BBFG_COMMAND_HEAD_TREE,
  BBFG_COMMAND_LIST_HEAD_TREE,
  BBFG_COMMAND_LIST_REFS,
  BBFG_COMMAND_LIST_REWRITE_REFS,
  BBFG_COMMAND_LIST_REWRITE_COMMITS,
  BBFG_COMMAND_REBUILD_HEAD_TREE,
  BBFG_COMMAND_REMOVE_HEAD_ENTRY,
  BBFG_COMMAND_COMMIT_WITHOUT_ENTRY
} BbfgCommand;

typedef struct
{
  BbfgCommand command;
  const char* path;
  const char* repo_path;
} BbfgOptions;

void
bbfg_print_usage(FILE* stream, const char* program_name);
int
bbfg_parse_options(BbfgOptions* options, int argc, char** argv);

#endif
