#include "cli.h"

#include <getopt.h>
#include <stddef.h>

typedef struct
{
  const char* name;
  int has_arg;
  int short_name;
  BbfgCommand command;
} BbfgCommandOption;

static const BbfgCommandOption command_options[] = {
  { "head-commit", no_argument, 'c', BBFG_COMMAND_HEAD_COMMIT },
  { "head-tree", no_argument, 't', BBFG_COMMAND_HEAD_TREE },
  { "list-head-tree", no_argument, 'T', BBFG_COMMAND_LIST_HEAD_TREE },
  { "list-refs", no_argument, 'r', BBFG_COMMAND_LIST_REFS },
  { "list-rewrite-refs", no_argument, 'R', BBFG_COMMAND_LIST_REWRITE_REFS },
  { "walk-rewrite-commits",
    no_argument,
    'w',
    BBFG_COMMAND_LIST_REWRITE_COMMITS },
  { "rebuild-head-tree", no_argument, 'B', BBFG_COMMAND_REBUILD_HEAD_TREE },
  { "remove-head-entry",
    required_argument,
    'd',
    BBFG_COMMAND_REMOVE_HEAD_ENTRY },
  { "commit-without-entry",
    required_argument,
    'C',
    BBFG_COMMAND_COMMIT_WITHOUT_ENTRY }
};

static const size_t command_options_count =
  sizeof(command_options) / sizeof(command_options[0]);

void
bbfg_print_usage(FILE* stream, const char* program_name)
{
  fprintf(stream,
          "usage: %s [-h|--help] [-c|--head-commit] [-t|--head-tree] "
          "[-T|--list-head-tree] [-r|--list-refs] [-R|--list-rewrite-refs] "
          "[-w|--walk-rewrite-commits] [-B|--rebuild-head-tree] "
          "[-d|--remove-head-entry path] [-C|--commit-without-entry path] "
          "<repo>\n",
          program_name);
}

static int
set_command(BbfgOptions* options, BbfgCommand command)
{
  if (options->command != BBFG_COMMAND_OPEN_REPO) {
    return -1;
  }

  options->command = command;
  return 0;
}

static const BbfgCommandOption*
find_command_option(int short_name)
{
  size_t i;

  for (i = 0; i < command_options_count; i++) {
    if (command_options[i].short_name == short_name) {
      return &command_options[i];
    }
  }

  return NULL;
}

static void
fill_long_options(struct option* long_options)
{
  size_t i;

  long_options[0].name = "help";
  long_options[0].has_arg = no_argument;
  long_options[0].flag = NULL;
  long_options[0].val = 'h';

  for (i = 0; i < command_options_count; i++) {
    long_options[i + 1].name = command_options[i].name;
    long_options[i + 1].has_arg = command_options[i].has_arg;
    long_options[i + 1].flag = NULL;
    long_options[i + 1].val = command_options[i].short_name;
  }

  long_options[command_options_count + 1].name = NULL;
  long_options[command_options_count + 1].has_arg = 0;
  long_options[command_options_count + 1].flag = NULL;
  long_options[command_options_count + 1].val = 0;
}

int
bbfg_parse_options(BbfgOptions* options, int argc, char** argv)
{
  struct option
    long_options[sizeof(command_options) / sizeof(command_options[0]) + 2];
  int option;

  fill_long_options(long_options);
  options->command = BBFG_COMMAND_OPEN_REPO;
  options->path = NULL;
  options->repo_path = NULL;
  opterr = 0;

  while ((option = getopt_long(
            argc, argv, "+hctTrRwBd:C:", long_options, NULL)) != -1) {
    const BbfgCommandOption* command_option;

    if (option == 'h') {
      return 1;
    }

    command_option = find_command_option(option);
    if (command_option == NULL) {
      return -1;
    }

    if (command_option->command == BBFG_COMMAND_REMOVE_HEAD_ENTRY ||
        command_option->command == BBFG_COMMAND_COMMIT_WITHOUT_ENTRY) {
      options->path = optarg;
    }

    if (set_command(options, command_option->command) < 0) {
      return -1;
    }
  }

  if (optind + 1 != argc) {
    return -1;
  }

  options->repo_path = argv[optind];
  return 0;
}
