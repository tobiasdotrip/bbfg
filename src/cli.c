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
    BBFG_COMMAND_COMMIT_WITHOUT_ENTRY },
  { "write-rewrite-ref",
    required_argument,
    'W',
    BBFG_COMMAND_WRITE_REWRITE_REF },
  { "rewrite-head-history",
    required_argument,
    'H',
    BBFG_COMMAND_REWRITE_HEAD_HISTORY },
  { "rewrite-ref", required_argument, 'u', BBFG_COMMAND_REWRITE_REF },
  { "rewrite-refs", no_argument, 'U', BBFG_COMMAND_REWRITE_REFS }
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
          "[-W|--write-rewrite-ref path] "
          "[-H|--rewrite-head-history path] "
          "[--rewrite-ref ref --delete path|--delete-files filename] "
          "[--rewrite-refs --delete path|--delete-files filename] "
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

static void
init_options(BbfgOptions* options)
{
  options->command = BBFG_COMMAND_OPEN_REPO;
  options->delete_mode = BBFG_DELETE_PATH;
  options->path = NULL;
  options->ref_name = NULL;
  options->repo_path = NULL;
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

static int
command_uses_path(BbfgCommand command)
{
  switch (command) {
    case BBFG_COMMAND_REMOVE_HEAD_ENTRY:
    case BBFG_COMMAND_COMMIT_WITHOUT_ENTRY:
    case BBFG_COMMAND_WRITE_REWRITE_REF:
    case BBFG_COMMAND_REWRITE_HEAD_HISTORY:
      return 1;
    default:
      return 0;
  }
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

  long_options[command_options_count + 1].name = "delete";
  long_options[command_options_count + 1].has_arg = required_argument;
  long_options[command_options_count + 1].flag = NULL;
  long_options[command_options_count + 1].val = 'D';

  long_options[command_options_count + 2].name = "delete-files";
  long_options[command_options_count + 2].has_arg = required_argument;
  long_options[command_options_count + 2].flag = NULL;
  long_options[command_options_count + 2].val = 'F';

  long_options[command_options_count + 3].name = NULL;
  long_options[command_options_count + 3].has_arg = 0;
  long_options[command_options_count + 3].flag = NULL;
  long_options[command_options_count + 3].val = 0;
}

static int
validate_options(const BbfgOptions* options, int delete_option)
{
  if (options->command == BBFG_COMMAND_REWRITE_REF ||
      options->command == BBFG_COMMAND_REWRITE_REFS) {
    if (!delete_option) {
      return 0;
    }

    return options->command == BBFG_COMMAND_REWRITE_REFS ||
           options->ref_name != NULL;
  }

  return delete_option == 0;
}

static int
parse_delete_option(BbfgOptions* options,
                    int option,
                    const char* argument,
                    int* delete_option)
{
  if (options->path != NULL) {
    return -1;
  }

  options->path = argument;
  options->delete_mode =
    option == 'F' ? BBFG_DELETE_FILENAME : BBFG_DELETE_PATH;
  *delete_option = 1;
  return 0;
}

static int
parse_command_option(BbfgOptions* options, int option, const char* argument)
{
  const BbfgCommandOption* command_option = find_command_option(option);
  if (command_option == NULL) {
    return -1;
  }

  if (command_uses_path(command_option->command)) {
    options->path = argument;
  }

  if (command_option->command == BBFG_COMMAND_REWRITE_REF) {
    options->ref_name = argument;
  }

  return set_command(options, command_option->command);
}

static int
parse_option(BbfgOptions* options,
             int option,
             const char* argument,
             int* delete_option)
{
  if (option == 'h') {
    return 1;
  }

  if (option == 'D' || option == 'F') {
    return parse_delete_option(options, option, argument, delete_option);
  }

  return parse_command_option(options, option, argument);
}

static int
finish_options(BbfgOptions* options, int argc, char** argv, int delete_option)
{
  if (optind + 1 != argc) {
    return -1;
  }

  options->repo_path = argv[optind];
  if (!validate_options(options, delete_option)) {
    return -1;
  }

  return 0;
}

int
bbfg_parse_options(BbfgOptions* options, int argc, char** argv)
{
  struct option
    long_options[sizeof(command_options) / sizeof(command_options[0]) + 4];
  int option;
  int delete_option = 0;

  fill_long_options(long_options);
  init_options(options);
  opterr = 0;

  while ((option = getopt_long(
            argc, argv, "+hctTrRwBd:C:W:H:u:UD:F:", long_options, NULL)) !=
         -1) {
    int result = parse_option(options, option, optarg, &delete_option);
    if (result != 0) {
      return result;
    }
  }

  return finish_options(options, argc, argv, delete_option);
}
