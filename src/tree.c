#include "tree.h"

#include <stdlib.h>
#include <string.h>

static int
copy_path_component(char** component, const char* path, size_t length)
{
  char* value = malloc(length + 1);
  if (value == NULL) {
    return -1;
  }

  memcpy(value, path, length);
  value[length] = '\0';
  *component = value;
  return 0;
}

static int
tree_is_empty(int* is_empty, git_repository* repo, const git_oid* tree_id)
{
  git_tree* tree = NULL;
  if (git_tree_lookup(&tree, repo, tree_id) < 0) {
    return -1;
  }

  *is_empty = git_tree_entrycount(tree) == 0;
  git_tree_free(tree);
  return 0;
}

static int
remove_tree_leaf(git_oid* rewritten_tree_id,
                 git_treebuilder* builder,
                 const char* path)
{
  int result = git_treebuilder_remove(builder, path);
  if (result == 0) {
    result = git_treebuilder_write(rewritten_tree_id, builder);
  }

  return result;
}

static int
replace_subtree_entry(git_treebuilder* builder,
                      git_repository* repo,
                      const git_tree_entry* entry,
                      const char* component,
                      const git_oid* rewritten_subtree_id)
{
  int is_empty = 0;
  int result = tree_is_empty(&is_empty, repo, rewritten_subtree_id);
  if (result < 0) {
    return result;
  }

  if (is_empty) {
    return git_treebuilder_remove(builder, component);
  }

  return git_treebuilder_insert(NULL,
                                builder,
                                component,
                                rewritten_subtree_id,
                                git_tree_entry_filemode(entry));
}

static int
remove_path_subtree(git_oid* rewritten_tree_id,
                    git_repository* repo,
                    git_tree* tree,
                    git_treebuilder* builder,
                    const char* path,
                    const char* slash)
{
  if (slash == path || slash[1] == '\0') {
    return -1;
  }

  char* component = NULL;
  if (copy_path_component(&component, path, (size_t)(slash - path)) < 0) {
    return -1;
  }

  const git_tree_entry* entry = git_tree_entry_byname(tree, component);
  if (entry == NULL || git_tree_entry_type(entry) != GIT_OBJECT_TREE) {
    free(component);
    return -1;
  }

  git_tree* subtree = NULL;
  if (git_tree_lookup(&subtree, repo, git_tree_entry_id(entry)) < 0) {
    free(component);
    return -1;
  }

  git_oid rewritten_subtree_id;
  int result =
    bbfg_tree_remove_path(&rewritten_subtree_id, repo, subtree, slash + 1);
  git_tree_free(subtree);

  if (result == 0) {
    result = replace_subtree_entry(
      builder, repo, entry, component, &rewritten_subtree_id);
  }

  if (result == 0) {
    result = git_treebuilder_write(rewritten_tree_id, builder);
  }

  free(component);
  return result;
}

int
bbfg_tree_remove_path(git_oid* rewritten_tree_id,
                      git_repository* repo,
                      git_tree* tree,
                      const char* path)
{
  git_treebuilder* builder = NULL;
  if (git_treebuilder_new(&builder, repo, tree) < 0) {
    return -1;
  }

  const char* slash = strchr(path, '/');
  int result = slash == NULL
                 ? remove_tree_leaf(rewritten_tree_id, builder, path)
                 : remove_path_subtree(
                     rewritten_tree_id, repo, tree, builder, path, slash);
  git_treebuilder_free(builder);
  return result;
}

static int
contains_filename_entry(int* matches,
                        git_repository* repo,
                        const git_tree_entry* entry,
                        const char* filename)
{
  git_object_t type = git_tree_entry_type(entry);
  if (type != GIT_OBJECT_TREE) {
    *matches = strcmp(git_tree_entry_name(entry), filename) == 0;
    return 0;
  }

  git_tree* subtree = NULL;
  if (git_tree_lookup(&subtree, repo, git_tree_entry_id(entry)) < 0) {
    return -1;
  }

  int result = bbfg_tree_contains_filename(matches, repo, subtree, filename);
  git_tree_free(subtree);
  return result;
}

int
bbfg_tree_contains_filename(int* matches,
                            git_repository* repo,
                            git_tree* tree,
                            const char* filename)
{
  size_t i;
  size_t entry_count = git_tree_entrycount(tree);

  *matches = 0;
  for (i = 0; i < entry_count; i++) {
    const git_tree_entry* entry = git_tree_entry_byindex(tree, i);
    int entry_matches = 0;
    int result = contains_filename_entry(&entry_matches, repo, entry, filename);
    if (result < 0) {
      return result;
    }

    if (entry_matches) {
      *matches = 1;
      return 0;
    }
  }

  return 0;
}

static int
remove_filename_entry(int* changed,
                      git_treebuilder* builder,
                      git_repository* repo,
                      const git_tree_entry* entry,
                      const char* filename)
{
  const char* entry_name = git_tree_entry_name(entry);
  git_object_t type = git_tree_entry_type(entry);
  *changed = 0;

  if (type != GIT_OBJECT_TREE && strcmp(entry_name, filename) == 0) {
    int result = git_treebuilder_remove(builder, entry_name);
    *changed = result == 0;
    return result;
  }

  if (type != GIT_OBJECT_TREE) {
    return 0;
  }

  git_tree* subtree = NULL;
  if (git_tree_lookup(&subtree, repo, git_tree_entry_id(entry)) < 0) {
    return -1;
  }

  git_oid rewritten_subtree_id;
  int result =
    bbfg_tree_remove_filename(&rewritten_subtree_id, repo, subtree, filename);
  git_tree_free(subtree);
  if (result < 0 ||
      git_oid_equal(&rewritten_subtree_id, git_tree_entry_id(entry))) {
    return result;
  }

  int is_empty = 0;
  result = tree_is_empty(&is_empty, repo, &rewritten_subtree_id);
  if (result == 0 && is_empty) {
    result = git_treebuilder_remove(builder, entry_name);
  } else if (result == 0) {
    result = git_treebuilder_insert(NULL,
                                    builder,
                                    entry_name,
                                    &rewritten_subtree_id,
                                    git_tree_entry_filemode(entry));
  }

  *changed = result == 0;
  return result;
}

int
bbfg_tree_remove_filename(git_oid* rewritten_tree_id,
                          git_repository* repo,
                          git_tree* tree,
                          const char* filename)
{
  git_treebuilder* builder = NULL;
  if (git_treebuilder_new(&builder, repo, tree) < 0) {
    return -1;
  }

  int changed = 0;
  size_t i;
  size_t entry_count = git_tree_entrycount(tree);
  for (i = 0; i < entry_count; i++) {
    int entry_changed = 0;
    int result = remove_filename_entry(
      &entry_changed, builder, repo, git_tree_entry_byindex(tree, i), filename);
    if (result < 0) {
      git_treebuilder_free(builder);
      return result;
    }

    changed = changed || entry_changed;
  }

  if (!changed) {
    git_oid_cpy(rewritten_tree_id, git_tree_id(tree));
    git_treebuilder_free(builder);
    return 0;
  }

  int result = git_treebuilder_write(rewritten_tree_id, builder);
  git_treebuilder_free(builder);
  return result;
}
