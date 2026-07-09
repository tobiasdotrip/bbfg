#include "cli.h"
#include "commands.h"
#include "error.h"

#include <git2.h>

#include <stdio.h>
#include <stdlib.h>

int
main(int argc, char** argv)
{
  const char* program_name = argv[0] != NULL ? argv[0] : "bbfg";
  BbfgOptions options;
  int parse_result = bbfg_parse_options(&options, argc, argv);

  if (parse_result > 0) {
    bbfg_print_usage(stdout, program_name);
    return EXIT_SUCCESS;
  }

  if (parse_result < 0) {
    bbfg_print_usage(stderr, program_name);
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
      command_result =
        bbfg_remove_head_tree_entry(repo, options.repo_path, options.path);
      break;
  }

  git_repository_free(repo);
  git_libgit2_shutdown();
  return command_result < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
