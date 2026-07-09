#include <git2.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
print_usage(FILE* stream, const char* program_name)
{
  fprintf(stream, "usage: %s [--help] <repo>\n", program_name);
}

int
main(int argc, char** argv)
{
  const char* program_name = argv[0] != NULL ? argv[0] : "bbfg";

  if (argc == 2 && strcmp(argv[1], "--help") == 0) {
    print_usage(stdout, program_name);
    return EXIT_SUCCESS;
  }

  if (argc < 2) {
    print_usage(stderr, program_name);
    return EXIT_FAILURE;
  }

  int init_count = git_libgit2_init();
  if (init_count < 0) {
    fprintf(stderr, "bbfg: failed to initialize libgit2\n");
    return EXIT_FAILURE;
  }

  fprintf(stderr, "bbfg: repository support is not wired yet: %s\n", argv[1]);
  git_libgit2_shutdown();
  return EXIT_FAILURE;
}
