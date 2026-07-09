#include "error.h"

#include <git2.h>

#include <stdio.h>

void
bbfg_print_git_error(const char* prefix, const char* detail)
{
  const git_error* error = git_error_last();
  const char* message =
    error->message != NULL ? error->message : "unknown libgit2 error";

  fprintf(stderr, "bbfg: %s: %s (%s)\n", prefix, detail, message);
}
