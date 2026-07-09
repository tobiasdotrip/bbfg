#include "commands.h"
#include "error.h"

#include <git2.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
print_usage(FILE* stream, const char* program_name)
{
  fprintf(
    stream,
    "usage: %s [--help] [--head-commit] [--head-tree] [--list-refs] <repo>\n",
    program_name);
}

int
main(int argc, char** argv)
{
  const char* program_name = argv[0] != NULL ? argv[0] : "bbfg";
  int head_commit = 0;
  int head_tree = 0;
  int list_refs = 0;
  const char* repo_path;

  if (argc == 2 && strcmp(argv[1], "--help") == 0) {
    print_usage(stdout, program_name);
    return EXIT_SUCCESS;
  }

  if (argc == 3 && strcmp(argv[1], "--head-commit") == 0) {
    head_commit = 1;
    repo_path = argv[2];
  } else if (argc == 3 && strcmp(argv[1], "--head-tree") == 0) {
    head_tree = 1;
    repo_path = argv[2];
  } else if (argc == 3 && strcmp(argv[1], "--list-refs") == 0) {
    list_refs = 1;
    repo_path = argv[2];
  } else if (argc == 2) {
    repo_path = argv[1];
  } else {
    print_usage(stderr, program_name);
    return EXIT_FAILURE;
  }

  int init_count = git_libgit2_init();
  if (init_count < 0) {
    fprintf(stderr, "bbfg: failed to initialize libgit2\n");
    return EXIT_FAILURE;
  }

  git_repository* repo = NULL;
  if (git_repository_open(&repo, repo_path) < 0) {
    bbfg_print_git_error("could not open repository", repo_path);
    git_libgit2_shutdown();
    return EXIT_FAILURE;
  }

  if (!head_commit && !head_tree && !list_refs) {
    printf("bbfg: using repository: %s\n", git_repository_path(repo));
  }

  if (head_commit && bbfg_print_head_commit_id(repo, repo_path) < 0) {
    git_repository_free(repo);
    git_libgit2_shutdown();
    return EXIT_FAILURE;
  }

  if (head_tree && bbfg_print_head_tree_id(repo, repo_path) < 0) {
    git_repository_free(repo);
    git_libgit2_shutdown();
    return EXIT_FAILURE;
  }

  if (list_refs && bbfg_print_refs(repo, repo_path) < 0) {
    git_repository_free(repo);
    git_libgit2_shutdown();
    return EXIT_FAILURE;
  }

  git_repository_free(repo);
  git_libgit2_shutdown();
  return EXIT_SUCCESS;
}
