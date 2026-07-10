#include "refs.h"

#include "error.h"

int
bbfg_write_ref(git_repository* repo,
               const char* ref_name,
               const git_oid* target_id,
               const char* log_message)
{
  git_reference* ref = NULL;
  if (git_reference_create(&ref, repo, ref_name, target_id, 1, log_message) <
      0) {
    bbfg_print_git_error("could not write ref", ref_name);
    return -1;
  }

  git_reference_free(ref);
  return 0;
}
