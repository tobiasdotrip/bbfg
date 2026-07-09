#include "commands.h"
#include "error.h"

#include <git2.h>

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

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
  BBFG_COMMAND_REMOVE_HEAD_ENTRY
} BbfgCommand;

typedef struct
{
  BbfgCommand command;
  const char* entry_name;
  const char* repo_path;
} BbfgOptions;

static void
print_usage(FILE* stream, const char* program_name)
{
  fprintf(stream,
          "usage: %s [-h|--help] [-c|--head-commit] [-t|--head-tree] "
          "[-T|--list-head-tree] [-r|--list-refs] [-R|--list-rewrite-refs] "
          "[-w|--walk-rewrite-commits] [-B|--rebuild-head-tree] "
          "[-d|--remove-head-entry name] "
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

static int
parse_options(BbfgOptions* options, int argc, char** argv)
{
  static const struct option long_options[] = {
    { "help", no_argument, NULL, 'h' },
    { "head-commit", no_argument, NULL, 'c' },
    { "head-tree", no_argument, NULL, 't' },
    { "list-head-tree", no_argument, NULL, 'T' },
    { "list-refs", no_argument, NULL, 'r' },
    { "list-rewrite-refs", no_argument, NULL, 'R' },
    { "walk-rewrite-commits", no_argument, NULL, 'w' },
    { "rebuild-head-tree", no_argument, NULL, 'B' },
    { "remove-head-entry", required_argument, NULL, 'd' },
    { NULL, 0, NULL, 0 }
  };
  int option;

  options->command = BBFG_COMMAND_OPEN_REPO;
  options->entry_name = NULL;
  options->repo_path = NULL;
  opterr = 0;

  while ((option =
            getopt_long(argc, argv, "+hctTrRwBd:", long_options, NULL)) != -1) {
    BbfgCommand command;

    switch (option) {
      case 'h':
        return 1;
      case 'c':
        command = BBFG_COMMAND_HEAD_COMMIT;
        break;
      case 't':
        command = BBFG_COMMAND_HEAD_TREE;
        break;
      case 'T':
        command = BBFG_COMMAND_LIST_HEAD_TREE;
        break;
      case 'r':
        command = BBFG_COMMAND_LIST_REFS;
        break;
      case 'R':
        command = BBFG_COMMAND_LIST_REWRITE_REFS;
        break;
      case 'w':
        command = BBFG_COMMAND_LIST_REWRITE_COMMITS;
        break;
      case 'B':
        command = BBFG_COMMAND_REBUILD_HEAD_TREE;
        break;
      case 'd':
        command = BBFG_COMMAND_REMOVE_HEAD_ENTRY;
        options->entry_name = optarg;
        break;
      default:
        return -1;
    }

    if (set_command(options, command) < 0) {
      return -1;
    }
  }

  if (optind + 1 != argc) {
    return -1;
  }

  options->repo_path = argv[optind];
  return 0;
}

int
main(int argc, char** argv)
{
  const char* program_name = argv[0] != NULL ? argv[0] : "bbfg";
  BbfgOptions options;
  int parse_result = parse_options(&options, argc, argv);

  if (parse_result > 0) {
    print_usage(stdout, program_name);
    return EXIT_SUCCESS;
  }

  if (parse_result < 0) {
    print_usage(stderr, program_name);
    return EXIT_FAILURE;
  }

  int init_count = git_libgit2_init();
  if (init_count < 0) {
    fprintf(stderr, "bbfg: failed to initialize libgit2\n");
    return EXIT_FAILURE;
  }

  git_repository* repo = NULL;
  if (git_repository_open(&repo, options.repo_path) < 0) {
    bbfg_print_git_error("could not open repository", options.repo_path);
    git_libgit2_shutdown();
    return EXIT_FAILURE;
  }

  int command_result = 0;
  switch (options.command) {
    case BBFG_COMMAND_OPEN_REPO:
      printf("bbfg: using repository: %s\n", git_repository_path(repo));
      break;
    case BBFG_COMMAND_HEAD_COMMIT:
      command_result = bbfg_print_head_commit_id(repo, options.repo_path);
      break;
    case BBFG_COMMAND_HEAD_TREE:
      command_result = bbfg_print_head_tree_id(repo, options.repo_path);
      break;
    case BBFG_COMMAND_LIST_HEAD_TREE:
      command_result = bbfg_print_head_tree_entries(repo, options.repo_path);
      break;
    case BBFG_COMMAND_LIST_REFS:
      command_result = bbfg_print_refs(repo, options.repo_path);
      break;
    case BBFG_COMMAND_LIST_REWRITE_REFS:
      command_result = bbfg_print_rewrite_refs(repo, options.repo_path);
      break;
    case BBFG_COMMAND_LIST_REWRITE_COMMITS:
      command_result = bbfg_print_rewrite_commits(repo, options.repo_path);
      break;
    case BBFG_COMMAND_REBUILD_HEAD_TREE:
      command_result = bbfg_rebuild_head_tree(repo, options.repo_path);
      break;
    case BBFG_COMMAND_REMOVE_HEAD_ENTRY:
      command_result = bbfg_remove_head_tree_entry(
        repo, options.repo_path, options.entry_name);
      break;
  }

  git_repository_free(repo);
  git_libgit2_shutdown();
  return command_result < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
