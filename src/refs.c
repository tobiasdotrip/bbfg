#include "refs.h"

#include "error.h"

#include <stdio.h>
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
validate_ref_name(const char* name)
{
  int valid = 0;
  if (name == NULL || git_reference_name_is_valid(&valid, name) < 0 || !valid ||
      !is_rewrite_namespace(name)) {
    fprintf(stderr,
            "bbfg: ref cannot be rewritten: %s\n",
            name == NULL ? "(null)" : name);
    return -1;
  }

  return 0;
}

static int
validate_existing_ref(git_repository* repo, const char* name, int allow_missing)
{
  git_reference* ref = NULL;
  int result = git_reference_lookup(&ref, repo, name);
  if (result < 0) {
    if (allow_missing && result == GIT_ENOTFOUND) {
      return 0;
    }

    bbfg_print_git_error("could not resolve ref", name);
    return -1;
  }

  if (git_reference_type(ref) != GIT_REFERENCE_DIRECT) {
    fprintf(stderr, "bbfg: cannot update symbolic ref: %s\n", name);
    git_reference_free(ref);
    return -1;
  }

  git_object* commit = NULL;
  result = git_reference_peel(&commit, ref, GIT_OBJECT_COMMIT);
  git_object_free(commit);
  git_reference_free(ref);
  if (result < 0) {
    bbfg_print_git_error("ref does not point to a commit", name);
    return -1;
  }

  return 0;
}

static int
is_rewriteable_ref(git_repository* repo, const char* name)
{
  git_reference* ref = NULL;
  if (git_reference_lookup(&ref, repo, name) < 0) {
    return 0;
  }

  if (git_reference_type(ref) != GIT_REFERENCE_DIRECT) {
    git_reference_free(ref);
    return 0;
  }

  git_object* commit = NULL;
  int result = git_reference_peel(&commit, ref, GIT_OBJECT_COMMIT) == 0;
  git_object_free(commit);
  git_reference_free(ref);
  return result;
}

static int
collect_rewrite_ref(const char* name, void* payload)
{
  CollectRewriteRefsPayload* collect = (CollectRewriteRefsPayload*)payload;

  if (!is_rewrite_namespace(name) || !is_rewriteable_ref(collect->repo, name)) {
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

int
bbfg_validate_rewrite_ref(git_repository* repo, const char* ref_name)
{
  if (validate_ref_name(ref_name) < 0) {
    return -1;
  }

  return validate_existing_ref(repo, ref_name, 0);
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
  BbfgRefUpdate update;
  update.name = ref_name;
  update.target_id = target_id;
  return bbfg_write_refs(repo, &update, 1, log_message);
}

static int
validate_ref_update(git_repository* repo, const BbfgRefUpdate* update)
{
  if (validate_ref_name(update->name) < 0) {
    return -1;
  }

  if (update->target_id == NULL) {
    fprintf(stderr, "bbfg: ref target is missing: %s\n", update->name);
    return -1;
  }

  git_object* target = NULL;
  if (git_object_lookup(&target, repo, update->target_id, GIT_OBJECT_ANY) < 0) {
    bbfg_print_git_error("ref target does not exist", update->name);
    return -1;
  }

  git_object_t target_type = git_object_type(target);
  int valid_target =
    has_prefix(update->name, "refs/heads/")
      ? target_type == GIT_OBJECT_COMMIT
      : target_type == GIT_OBJECT_COMMIT || target_type == GIT_OBJECT_TAG;
  git_object_free(target);
  if (!valid_target) {
    fprintf(stderr, "bbfg: invalid object type for ref: %s\n", update->name);
    return -1;
  }

  return validate_existing_ref(repo, update->name, 1);
}

static int
validate_ref_updates(git_repository* repo,
                     const BbfgRefUpdate* updates,
                     size_t update_count)
{
  size_t i;

  for (i = 0; i < update_count; i++) {
    if (validate_ref_update(repo, &updates[i]) < 0) {
      return -1;
    }
  }

  return 0;
}

static int
lock_ref_updates(git_transaction* transaction,
                 const BbfgRefUpdate* updates,
                 size_t update_count)
{
  size_t i;

  for (i = 0; i < update_count; i++) {
    if (git_transaction_lock_ref(transaction, updates[i].name) < 0) {
      bbfg_print_git_error("could not lock ref", updates[i].name);
      return -1;
    }
  }

  return 0;
}

static int
validate_locked_refs(git_repository* repo,
                     const BbfgRefUpdate* updates,
                     size_t update_count)
{
  size_t i;

  for (i = 0; i < update_count; i++) {
    if (validate_existing_ref(repo, updates[i].name, 1) < 0) {
      return -1;
    }
  }

  return 0;
}

static int
queue_ref_updates(git_transaction* transaction,
                  const BbfgRefUpdate* updates,
                  size_t update_count,
                  const char* log_message)
{
  size_t i;

  for (i = 0; i < update_count; i++) {
    if (git_transaction_set_target(transaction,
                                   updates[i].name,
                                   updates[i].target_id,
                                   NULL,
                                   log_message) < 0) {
      bbfg_print_git_error("could not queue ref update", updates[i].name);
      return -1;
    }
  }

  return 0;
}

int
bbfg_write_refs(git_repository* repo,
                const BbfgRefUpdate* updates,
                size_t update_count,
                const char* log_message)
{
  if (update_count == 0) {
    return 0;
  }

  if (updates == NULL) {
    fprintf(stderr, "bbfg: ref updates are missing\n");
    return -1;
  }

  if (validate_ref_updates(repo, updates, update_count) < 0) {
    return -1;
  }

  git_transaction* transaction = NULL;
  if (git_transaction_new(&transaction, repo) < 0) {
    bbfg_print_git_error("could not start ref transaction", "refs");
    return -1;
  }

  if (lock_ref_updates(transaction, updates, update_count) < 0) {
    git_transaction_free(transaction);
    return -1;
  }

  if (validate_locked_refs(repo, updates, update_count) < 0) {
    git_transaction_free(transaction);
    return -1;
  }

  if (queue_ref_updates(transaction, updates, update_count, log_message) < 0) {
    git_transaction_free(transaction);
    return -1;
  }

  int result = git_transaction_commit(transaction);
  if (result < 0) {
    bbfg_print_git_error("could not commit ref transaction", "refs");
  }
  git_transaction_free(transaction);
  return result;
}
