#include "refs.h"

#include "error.h"

#include <stdlib.h>
#include <string.h>

typedef struct
{
  BbfgRefList* refs;
  git_repository* repo;
} CollectRewriteRefsPayload;

static int
has_prefix(const char* value, const char* prefix)
{
  return strncmp(value, prefix, strlen(prefix)) == 0;
}

static int
append_ref_name(BbfgRefList* refs, const char* name)
{
  if (refs->count == refs->capacity) {
    size_t next_capacity = refs->capacity == 0 ? 16 : refs->capacity * 2;
    char** next_names =
      (char**)realloc((void*)refs->names, next_capacity * sizeof(*next_names));
    if (next_names == NULL) {
      return -1;
    }

    refs->names = next_names;
    refs->capacity = next_capacity;
  }

  size_t name_length = strlen(name);
  refs->names[refs->count] = (char*)malloc(name_length + 1);
  if (refs->names[refs->count] == NULL) {
    return -1;
  }

  memcpy(refs->names[refs->count], name, name_length + 1);
  refs->count++;
  return 0;
}

static int
is_rewrite_namespace(const char* name)
{
  return has_prefix(name, "refs/heads/") || has_prefix(name, "refs/tags/");
}

static int
is_direct_commit_ref(git_repository* repo, const char* name)
{
  git_reference* ref = NULL;
  if (git_reference_lookup(&ref, repo, name) < 0) {
    return 0;
  }

  if (git_reference_type(ref) != GIT_REFERENCE_DIRECT) {
    git_reference_free(ref);
    return 0;
  }

  const git_oid* target_id = git_reference_target(ref);
  git_commit* commit = NULL;
  int result =
    target_id != NULL && git_commit_lookup(&commit, repo, target_id) == 0;
  if (commit != NULL) {
    git_commit_free(commit);
  }
  git_reference_free(ref);
  return result;
}

static int
collect_rewrite_ref(const char* name, void* payload)
{
  CollectRewriteRefsPayload* collect = (CollectRewriteRefsPayload*)payload;

  if (!is_rewrite_namespace(name) ||
      !is_direct_commit_ref(collect->repo, name)) {
    return 0;
  }

  return append_ref_name(collect->refs, name);
}

int
bbfg_collect_rewrite_refs(BbfgRefList* refs, git_repository* repo)
{
  CollectRewriteRefsPayload payload;
  payload.refs = refs;
  payload.repo = repo;

  if (git_reference_foreach_name(repo, collect_rewrite_ref, &payload) < 0) {
    bbfg_print_git_error("could not collect rewrite refs", "refs");
    return -1;
  }

  return 0;
}

void
bbfg_ref_list_dispose(BbfgRefList* refs)
{
  size_t i;

  for (i = 0; i < refs->count; i++) {
    free(refs->names[i]);
  }

  free((void*)refs->names);
  refs->names = NULL;
  refs->count = 0;
  refs->capacity = 0;
}

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
