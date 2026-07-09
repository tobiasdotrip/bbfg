#include <git2.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
print_usage(FILE* stream, const char* program_name)
{
  fprintf(stream, "usage: %s [--help] [--list-refs] <repo>\n", program_name);
}

static void
print_git_error(const char* prefix, const char* detail)
{
  const git_error* error = git_error_last();
  const char* message =
    error->message != NULL ? error->message : "unknown libgit2 error";

  fprintf(stderr, "bbfg: %s: %s (%s)\n", prefix, detail, message);
}

static int
print_ref_name(const char* name, void* payload)
{
  (void)payload;
  printf("%s\n", name);
  return 0;
}

int
main(int argc, char** argv)
{
  const char* program_name = argv[0] != NULL ? argv[0] : "bbfg";
  int list_refs = 0;
  const char* repo_path;

  if (argc == 2 && strcmp(argv[1], "--help") == 0) {
    print_usage(stdout, program_name);
    return EXIT_SUCCESS;
  }

  if (argc == 3 && strcmp(argv[1], "--list-refs") == 0) {
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
    print_git_error("could not open repository", repo_path);
    git_libgit2_shutdown();
    return EXIT_FAILURE;
  }

  if (!list_refs) {
    printf("bbfg: using repository: %s\n", git_repository_path(repo));
  }

  if (list_refs && git_reference_foreach_name(repo, print_ref_name, NULL) < 0) {
    print_git_error("could not list refs", repo_path);
    git_repository_free(repo);
    git_libgit2_shutdown();
    return EXIT_FAILURE;
  }

  git_repository_free(repo);
  git_libgit2_shutdown();
  return EXIT_SUCCESS;
}
