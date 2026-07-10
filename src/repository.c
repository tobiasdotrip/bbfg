#include "repository.h"

#include "error.h"

int
bbfg_lookup_head_commit(git_commit** commit,
                        git_repository* repo,
                        const char* repo_path)
{
  git_reference* head = NULL;
  if (git_repository_head(&head, repo) < 0) {
    bbfg_print_git_error("could not resolve HEAD", repo_path);
    return -1;
  }

  git_object* head_object = NULL;
  if (git_reference_peel(&head_object, head, GIT_OBJECT_COMMIT) < 0) {
    bbfg_print_git_error("HEAD does not point to a commit", repo_path);
    git_reference_free(head);
    return -1;
  }

  *commit = (git_commit*)head_object;
  git_reference_free(head);
  return 0;
}

int
bbfg_lookup_ref_commit(git_commit** commit,
                       git_repository* repo,
                       const char* ref_name)
{
  git_reference* ref = NULL;
  if (git_reference_lookup(&ref, repo, ref_name) < 0) {
    bbfg_print_git_error("could not resolve ref", ref_name);
    return -1;
  }

  git_object* object = NULL;
  if (git_reference_peel(&object, ref, GIT_OBJECT_COMMIT) < 0) {
    bbfg_print_git_error("ref does not point to a commit", ref_name);
    git_reference_free(ref);
    return -1;
  }

  *commit = (git_commit*)object;
  git_reference_free(ref);
  return 0;
}
