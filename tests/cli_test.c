#include <criterion/criterion.h>

#include "test_helpers.h"

#include <stdlib.h>
#include <string.h>

Test(cli, repository_commands)
{
  char tmpdir[BBFG_TEST_PATH_SIZE];
  char repo[BBFG_TEST_PATH_SIZE];
  const char* bbfg = bbfg_test_path();

  bbfg_test_init_repo(tmpdir, sizeof(tmpdir), repo, sizeof(repo));

  char* git_dir =
    bbfg_test_read_command("git -C %s rev-parse --absolute-git-dir", repo);
  bbfg_test_strip_trailing_newline(git_dir);
  char* expected_open =
    bbfg_test_format_string("bbfg: using repository: %s/\n", git_dir);
  char* actual = bbfg_test_read_command("%s %s", bbfg, repo);
  cr_assert_str_eq(actual, expected_open);
  free(actual);
  free(expected_open);
  free(git_dir);

  actual = bbfg_test_read_command("%s --help", bbfg);
  cr_assert(strncmp(actual, "usage: ", strlen("usage: ")) == 0);
  free(actual);

  cr_assert_neq(
    bbfg_test_run_command(
      "%s --head-commit --head-tree %s >/dev/null 2>&1", bbfg, repo),
    0);
  cr_assert_neq(
    bbfg_test_run_command("%s %s --head-commit >/dev/null 2>&1", bbfg, repo),
    0);

  char* expected_head =
    bbfg_test_read_command("git -C %s rev-parse HEAD", repo);
  actual = bbfg_test_read_command("%s --head-commit %s", bbfg, repo);
  cr_assert_str_eq(actual, expected_head);
  free(actual);

  char* expected =
    bbfg_test_read_command("git -C %s rev-parse 'HEAD^{tree}'", repo);
  actual = bbfg_test_read_command("%s --head-tree %s", bbfg, repo);
  cr_assert_str_eq(actual, expected);
  free(actual);

  actual = bbfg_test_read_command("%s --rebuild-head-tree %s", bbfg, repo);
  cr_assert_str_eq(actual, expected);
  free(actual);
  free(expected);

  expected = bbfg_test_read_command("git -C %s ls-tree HEAD", repo);
  actual = bbfg_test_read_command("%s --list-head-tree %s", bbfg, repo);
  cr_assert_str_eq(actual, expected);
  free(actual);
  free(expected);

  free(expected_head);
  bbfg_test_cleanup(tmpdir);
}

Test(cli, tree_commands)
{
  char tmpdir[BBFG_TEST_PATH_SIZE];
  char repo[BBFG_TEST_PATH_SIZE];
  const char* bbfg = bbfg_test_path();

  bbfg_test_init_repo(tmpdir, sizeof(tmpdir), repo, sizeof(repo));

  char* actual =
    bbfg_test_read_command("%s --remove-head-entry file.txt %s", bbfg, repo);
  cr_assert_eq(bbfg_test_run_command("git -C %s read-tree HEAD", repo), 0);
  cr_assert_eq(bbfg_test_run_command("git -C %s rm -q --cached file.txt", repo),
               0);
  char* expected = bbfg_test_read_command("git -C %s write-tree", repo);
  cr_assert_str_eq(actual, expected);
  free(actual);
  free(expected);

  actual = bbfg_test_read_command(
    "%s --remove-head-entry dir/nested.txt %s", bbfg, repo);
  cr_assert_eq(bbfg_test_run_command("git -C %s read-tree HEAD", repo), 0);
  cr_assert_eq(
    bbfg_test_run_command("git -C %s rm -q --cached dir/nested.txt", repo), 0);
  expected = bbfg_test_read_command("git -C %s write-tree", repo);
  cr_assert_str_eq(actual, expected);
  free(actual);
  free(expected);

  bbfg_test_cleanup(tmpdir);
}

Test(cli, commit_command)
{
  char tmpdir[BBFG_TEST_PATH_SIZE];
  char repo[BBFG_TEST_PATH_SIZE];
  const char* bbfg = bbfg_test_path();

  bbfg_test_init_repo(tmpdir, sizeof(tmpdir), repo, sizeof(repo));

  char* expected_head =
    bbfg_test_read_command("git -C %s rev-parse HEAD", repo);
  char* expected_tree = bbfg_test_read_command(
    "git -C %s read-tree HEAD && git -C %s rm -q --cached dir/nested.txt "
    "&& git -C %s write-tree",
    repo,
    repo,
    repo);
  char* commit_id = bbfg_test_read_command(
    "%s --commit-without-entry dir/nested.txt %s", bbfg, repo);
  bbfg_test_strip_trailing_newline(commit_id);

  char* actual =
    bbfg_test_read_command("git -C %s cat-file -t %s", repo, commit_id);
  cr_assert_str_eq(actual, "commit\n");
  free(actual);

  actual =
    bbfg_test_read_command("git -C %s rev-parse '%s^{tree}'", repo, commit_id);
  cr_assert_str_eq(actual, expected_tree);
  free(actual);

  actual = bbfg_test_read_command("git -C %s rev-parse HEAD", repo);
  cr_assert_str_eq(actual, expected_head);
  free(actual);
  free(commit_id);
  free(expected_tree);
  free(expected_head);
  bbfg_test_cleanup(tmpdir);
}

Test(cli, rewrite_history_commands)
{
  char tmpdir[BBFG_TEST_PATH_SIZE];
  char repo[BBFG_TEST_PATH_SIZE];
  const char* bbfg = bbfg_test_path();

  bbfg_test_init_repo(tmpdir, sizeof(tmpdir), repo, sizeof(repo));

  char* expected_head =
    bbfg_test_read_command("git -C %s rev-parse HEAD", repo);
  char* expected_tree = bbfg_test_read_command(
    "git -C %s read-tree HEAD && git -C %s rm -q --cached dir/nested.txt "
    "&& git -C %s write-tree",
    repo,
    repo,
    repo);

  char* rewrite_ref = bbfg_test_read_command(
    "%s --write-rewrite-ref dir/nested.txt %s", bbfg, repo);
  bbfg_test_strip_trailing_newline(rewrite_ref);
  char* expected =
    bbfg_test_read_command("git -C %s rev-parse refs/heads/bbfg-rewrite", repo);
  bbfg_test_strip_trailing_newline(expected);
  cr_assert_str_eq(rewrite_ref, expected);
  free(expected);

  char* actual = bbfg_test_read_command(
    "git -C %s rev-parse '%s^{tree}'", repo, rewrite_ref);
  cr_assert_str_eq(actual, expected_tree);
  free(actual);

  actual = bbfg_test_read_command("git -C %s rev-parse HEAD", repo);
  cr_assert_str_eq(actual, expected_head);
  free(actual);

  char* history_ref = bbfg_test_read_command(
    "%s --rewrite-head-history dir/nested.txt --no-blob-protection %s",
    bbfg,
    repo);
  bbfg_test_strip_trailing_newline(history_ref);
  expected =
    bbfg_test_read_command("git -C %s rev-parse refs/heads/bbfg-rewrite", repo);
  bbfg_test_strip_trailing_newline(expected);
  cr_assert_str_eq(history_ref, expected);
  free(expected);

  actual = bbfg_test_read_command(
    "git -C %s rev-parse '%s^{tree}'", repo, history_ref);
  cr_assert_str_eq(actual, expected_tree);
  free(actual);

  expected =
    bbfg_test_read_command("git -C %s rev-list --count refs/heads/main", repo);
  actual = bbfg_test_read_command(
    "git -C %s rev-list --count refs/heads/bbfg-rewrite", repo);
  cr_assert_str_eq(actual, expected);
  free(actual);
  free(expected);

  actual = bbfg_test_read_command("git -C %s rev-parse HEAD", repo);
  cr_assert_str_eq(actual, expected_head);
  free(actual);

  free(expected_tree);
  free(expected_head);
  free(rewrite_ref);
  free(history_ref);
  bbfg_test_cleanup(tmpdir);
}

Test(cli, rewrite_ref_command)
{
  char tmpdir[BBFG_TEST_PATH_SIZE];
  char repo[BBFG_TEST_PATH_SIZE];
  const char* bbfg = bbfg_test_path();

  bbfg_test_init_repo(tmpdir, sizeof(tmpdir), repo, sizeof(repo));
  cr_assert_eq(bbfg_test_run_command("git -C %s branch target", repo), 0);

  char* expected_head =
    bbfg_test_read_command("git -C %s rev-parse HEAD", repo);
  char* target_before =
    bbfg_test_read_command("git -C %s rev-parse refs/heads/target", repo);
  bbfg_test_strip_trailing_newline(target_before);
  char* rewritten_target = bbfg_test_read_command(
    "%s --rewrite-ref refs/heads/target --delete dir/nested.txt "
    "--no-blob-protection %s",
    bbfg,
    repo);
  bbfg_test_strip_trailing_newline(rewritten_target);
  char* expected =
    bbfg_test_read_command("git -C %s rev-parse refs/heads/target", repo);
  bbfg_test_strip_trailing_newline(expected);
  cr_assert_str_eq(rewritten_target, expected);
  cr_assert_str_neq(rewritten_target, target_before);
  free(expected);

  char* expected_tree = bbfg_test_read_command(
    "git -C %s read-tree HEAD && git -C %s rm -q --cached dir/nested.txt "
    "&& git -C %s write-tree",
    repo,
    repo,
    repo);
  char* actual = bbfg_test_read_command(
    "git -C %s rev-parse '%s^{tree}'", repo, rewritten_target);
  cr_assert_str_eq(actual, expected_tree);
  free(actual);

  actual = bbfg_test_read_command("git -C %s rev-parse HEAD", repo);
  cr_assert_str_eq(actual, expected_head);
  free(actual);

  free(expected_head);
  free(expected_tree);
  free(target_before);
  free(rewritten_target);
  bbfg_test_cleanup(tmpdir);
}

Test(cli, rewrite_ref_rejects_unsafe_refs)
{
  char tmpdir[BBFG_TEST_PATH_SIZE];
  char repo[BBFG_TEST_PATH_SIZE];
  const char* bbfg = bbfg_test_path();

  bbfg_test_init_repo(tmpdir, sizeof(tmpdir), repo, sizeof(repo));

  char* before_head =
    bbfg_test_read_command("git -C %s symbolic-ref HEAD", repo);
  char* before_commit =
    bbfg_test_read_command("git -C %s rev-parse HEAD", repo);
  cr_assert_neq(
    bbfg_test_run_command("%s --rewrite-ref HEAD --delete dir/nested.txt %s "
                          ">/dev/null 2>&1",
                          bbfg,
                          repo),
    0);
  cr_assert_neq(
    bbfg_test_run_command("%s --rewrite-ref refs/remotes/origin/main --delete "
                          "dir/nested.txt %s >/dev/null 2>&1",
                          bbfg,
                          repo),
    0);
  cr_assert_neq(
    bbfg_test_run_command("%s --rewrite-ref refs/heads/main..bad --delete "
                          "dir/nested.txt %s >/dev/null 2>&1",
                          bbfg,
                          repo),
    0);

  char* actual = bbfg_test_read_command("git -C %s symbolic-ref HEAD", repo);
  cr_assert_str_eq(actual, before_head);
  free(actual);

  actual = bbfg_test_read_command("git -C %s rev-parse HEAD", repo);
  cr_assert_str_eq(actual, before_commit);
  free(actual);

  free(before_commit);
  free(before_head);
  bbfg_test_cleanup(tmpdir);
}

Test(cli, rewrite_annotated_tag)
{
  char tmpdir[BBFG_TEST_PATH_SIZE];
  char repo[BBFG_TEST_PATH_SIZE];
  const char* bbfg = bbfg_test_path();

  bbfg_test_init_repo(tmpdir, sizeof(tmpdir), repo, sizeof(repo));
  bbfg_test_add_annotated_tag(repo);

  char* before_tag =
    bbfg_test_read_command("git -C %s rev-parse refs/tags/v1", repo);
  bbfg_test_strip_trailing_newline(before_tag);
  char* rewritten = bbfg_test_read_command(
    "%s --rewrite-ref refs/tags/v1 --delete dir/nested.txt "
    "--no-blob-protection %s",
    bbfg,
    repo);
  bbfg_test_strip_trailing_newline(rewritten);

  char* after_tag =
    bbfg_test_read_command("git -C %s rev-parse refs/tags/v1", repo);
  bbfg_test_strip_trailing_newline(after_tag);
  cr_assert_str_neq(after_tag, before_tag);
  cr_assert_str_eq(rewritten, after_tag);

  char* type =
    bbfg_test_read_command("git -C %s cat-file -t refs/tags/v1", repo);
  cr_assert_str_eq(type, "tag\n");
  free(type);

  char* target =
    bbfg_test_read_command("git -C %s rev-parse 'refs/tags/v1^{commit}'", repo);
  bbfg_test_strip_trailing_newline(target);
  char* names =
    bbfg_test_read_command("git -C %s ls-tree -r --name-only %s", repo, target);
  cr_assert_null(strstr(names, "dir/nested.txt"));
  cr_assert_not_null(strstr(names, "other/nested.txt"));
  free(names);

  char* tag_body =
    bbfg_test_read_command("git -C %s cat-file -p refs/tags/v1", repo);
  cr_assert_not_null(strstr(tag_body, "tag v1\n"));
  cr_assert_not_null(strstr(tag_body, "Release v1\n"));
  free(tag_body);

  cr_assert_eq(bbfg_test_run_command(
                 "git -C %s fsck --full --no-progress >/dev/null", repo),
               0);

  free(target);
  free(after_tag);
  free(rewritten);
  free(before_tag);
  bbfg_test_cleanup(tmpdir);
}

Test(cli, rewrite_nested_annotated_tags)
{
  char tmpdir[BBFG_TEST_PATH_SIZE];
  char repo[BBFG_TEST_PATH_SIZE];
  const char* bbfg = bbfg_test_path();

  bbfg_test_init_repo(tmpdir, sizeof(tmpdir), repo, sizeof(repo));
  bbfg_test_add_nested_annotated_tags(repo);

  char* before_inner =
    bbfg_test_read_command("git -C %s rev-parse refs/tags/inner", repo);
  bbfg_test_strip_trailing_newline(before_inner);
  char* before_outer =
    bbfg_test_read_command("git -C %s rev-parse refs/tags/outer", repo);
  bbfg_test_strip_trailing_newline(before_outer);

  char* output = bbfg_test_read_command(
    "%s --rewrite-refs --delete dir/nested.txt --no-blob-protection %s",
    bbfg,
    repo);
  free(output);

  char* after_inner =
    bbfg_test_read_command("git -C %s rev-parse refs/tags/inner", repo);
  bbfg_test_strip_trailing_newline(after_inner);
  char* after_outer =
    bbfg_test_read_command("git -C %s rev-parse refs/tags/outer", repo);
  bbfg_test_strip_trailing_newline(after_outer);
  cr_assert_str_neq(after_inner, before_inner);
  cr_assert_str_neq(after_outer, before_outer);

  char* outer_target = bbfg_test_read_command(
    "git -C %s cat-file -p %s | sed -n 's/^object //p'", repo, after_outer);
  bbfg_test_strip_trailing_newline(outer_target);
  char* outer_target_type =
    bbfg_test_read_command("git -C %s cat-file -t %s", repo, outer_target);
  cr_assert_str_eq(outer_target_type, "tag\n");
  cr_assert_str_eq(after_inner, outer_target);
  free(outer_target_type);

  char* inner_target = bbfg_test_read_command(
    "git -C %s cat-file -p %s | sed -n 's/^object //p'", repo, outer_target);
  bbfg_test_strip_trailing_newline(inner_target);
  char* inner_target_type =
    bbfg_test_read_command("git -C %s cat-file -t %s", repo, inner_target);
  cr_assert_str_eq(inner_target_type, "commit\n");
  free(inner_target_type);

  char* names = bbfg_test_read_command(
    "git -C %s ls-tree -r --name-only %s", repo, inner_target);
  cr_assert_null(strstr(names, "dir/nested.txt"));
  cr_assert_not_null(strstr(names, "file.txt"));
  free(names);

  cr_assert_eq(bbfg_test_run_command(
                 "git -C %s fsck --full --no-progress >/dev/null", repo),
               0);

  free(inner_target);
  free(outer_target);
  free(after_outer);
  free(after_inner);
  free(before_outer);
  free(before_inner);
  bbfg_test_cleanup(tmpdir);
}

Test(cli, rewrite_combined_filters)
{
  char tmpdir[BBFG_TEST_PATH_SIZE];
  char repo[BBFG_TEST_PATH_SIZE];
  const char* bbfg = bbfg_test_path();

  bbfg_test_init_repo(tmpdir, sizeof(tmpdir), repo, sizeof(repo));
  bbfg_test_add_large_file(repo);

  char* rewritten = bbfg_test_read_command(
    "%s --rewrite-ref refs/heads/main --delete-files nested.txt "
    "--strip-blobs-bigger-than 1K --no-blob-protection %s",
    bbfg,
    repo);
  bbfg_test_strip_trailing_newline(rewritten);

  char* names = bbfg_test_read_command(
    "git -C %s ls-tree -r --name-only refs/heads/main", repo);
  cr_assert_null(strstr(names, "nested.txt"));
  cr_assert_null(strstr(names, "large.bin"));
  cr_assert_not_null(strstr(names, "file.txt"));
  free(names);

  char* type =
    bbfg_test_read_command("git -C %s cat-file -t %s", repo, rewritten);
  cr_assert_str_eq(type, "commit\n");
  free(type);

  cr_assert_eq(bbfg_test_run_command(
                 "git -C %s fsck --full --no-progress >/dev/null", repo),
               0);

  free(rewritten);
  bbfg_test_cleanup(tmpdir);
}

Test(cli, rewrite_protects_tip_blobs)
{
  char tmpdir[BBFG_TEST_PATH_SIZE];
  char repo[BBFG_TEST_PATH_SIZE];
  const char* bbfg = bbfg_test_path();

  bbfg_test_init_repo(tmpdir, sizeof(tmpdir), repo, sizeof(repo));
  bbfg_test_add_large_file_history(repo);

  char* rewritten = bbfg_test_read_command(
    "%s --rewrite-ref refs/heads/main --strip-blobs-bigger-than 1K %s",
    bbfg,
    repo);
  bbfg_test_strip_trailing_newline(rewritten);

  char* tip_names = bbfg_test_read_command(
    "git -C %s ls-tree -r --name-only refs/heads/main", repo);
  cr_assert_not_null(strstr(tip_names, "large.bin"));
  free(tip_names);

  char* parent_names = bbfg_test_read_command(
    "git -C %s ls-tree -r --name-only refs/heads/main^", repo);
  cr_assert_null(strstr(parent_names, "large.bin"));
  free(parent_names);

  free(rewritten);
  bbfg_test_cleanup(tmpdir);
}

Test(cli, rewrite_no_blob_protection)
{
  char tmpdir[BBFG_TEST_PATH_SIZE];
  char repo[BBFG_TEST_PATH_SIZE];
  const char* bbfg = bbfg_test_path();

  bbfg_test_init_repo(tmpdir, sizeof(tmpdir), repo, sizeof(repo));
  bbfg_test_add_large_file(repo);

  char* rewritten = bbfg_test_read_command(
    "%s --rewrite-ref refs/heads/main --strip-blobs-bigger-than 1K "
    "--no-blob-protection %s",
    bbfg,
    repo);
  bbfg_test_strip_trailing_newline(rewritten);

  char* names = bbfg_test_read_command(
    "git -C %s ls-tree -r --name-only refs/heads/main", repo);
  cr_assert_null(strstr(names, "large.bin"));
  free(names);

  free(rewritten);
  bbfg_test_cleanup(tmpdir);
}

Test(cli, rewrite_refs_protects_each_tip)
{
  char tmpdir[BBFG_TEST_PATH_SIZE];
  char repo[BBFG_TEST_PATH_SIZE];
  const char* bbfg = bbfg_test_path();

  bbfg_test_init_repo(tmpdir, sizeof(tmpdir), repo, sizeof(repo));
  cr_assert_eq(
    bbfg_test_run_command("git -C %s branch -q feature HEAD~1", repo), 0);
  bbfg_test_add_large_file(repo);
  cr_assert_eq(bbfg_test_run_command("git -C %s checkout -q feature", repo), 0);
  bbfg_test_add_large_file(repo);
  cr_assert_eq(bbfg_test_run_command("git -C %s checkout -q main", repo), 0);

  char* output = bbfg_test_read_command(
    "%s --rewrite-refs --strip-blobs-bigger-than 1K %s", bbfg, repo);
  free(output);

  char* main_names = bbfg_test_read_command(
    "git -C %s ls-tree -r --name-only refs/heads/main", repo);
  cr_assert_not_null(strstr(main_names, "large.bin"));
  free(main_names);

  char* feature_names = bbfg_test_read_command(
    "git -C %s ls-tree -r --name-only refs/heads/feature", repo);
  cr_assert_not_null(strstr(feature_names, "large.bin"));
  free(feature_names);

  bbfg_test_cleanup(tmpdir);
}

Test(cli, rewrite_merge_history)
{
  char tmpdir[BBFG_TEST_PATH_SIZE];
  char repo[BBFG_TEST_PATH_SIZE];
  const char* bbfg = bbfg_test_path();

  bbfg_test_init_repo(tmpdir, sizeof(tmpdir), repo, sizeof(repo));
  bbfg_test_add_merge_history(repo);

  char* before_count =
    bbfg_test_read_command("git -C %s rev-list --count refs/heads/main", repo);
  char* before_target =
    bbfg_test_read_command("git -C %s rev-parse refs/heads/main", repo);
  bbfg_test_strip_trailing_newline(before_target);

  char* rewritten = bbfg_test_read_command(
    "%s --rewrite-ref refs/heads/main --delete dir/nested.txt "
    "--no-blob-protection %s",
    bbfg,
    repo);
  bbfg_test_strip_trailing_newline(rewritten);
  cr_assert_str_neq(rewritten, before_target);

  char* actual_count =
    bbfg_test_read_command("git -C %s rev-list --count refs/heads/main", repo);
  cr_assert_str_eq(actual_count, before_count);
  free(actual_count);
  free(before_count);

  char* names = bbfg_test_read_command(
    "git -C %s ls-tree -r --name-only refs/heads/main", repo);
  cr_assert_null(strstr(names, "dir/nested.txt"));
  cr_assert_not_null(strstr(names, "feature.txt"));
  free(names);

  char* feature_names = bbfg_test_read_command(
    "git -C %s ls-tree -r --name-only refs/heads/feature", repo);
  cr_assert_not_null(strstr(feature_names, "dir/nested.txt"));
  free(feature_names);

  cr_assert_eq(bbfg_test_run_command(
                 "git -C %s fsck --full --no-progress >/dev/null", repo),
               0);

  free(rewritten);
  free(before_target);
  bbfg_test_cleanup(tmpdir);
}

Test(cli, rewrite_refs_command)
{
  char tmpdir[BBFG_TEST_PATH_SIZE];
  char repo[BBFG_TEST_PATH_SIZE];
  const char* bbfg = bbfg_test_path();

  bbfg_test_init_repo(tmpdir, sizeof(tmpdir), repo, sizeof(repo));

  char* expected_tree = bbfg_test_read_command(
    "git -C %s read-tree HEAD && git -C %s rm -q --cached dir/nested.txt "
    "&& git -C %s write-tree",
    repo,
    repo,
    repo);
  char* rewritten_refs = bbfg_test_read_command(
    "%s --rewrite-refs --delete dir/nested.txt --no-blob-protection %s",
    bbfg,
    repo);
  char* expected = bbfg_test_read_command(
    "git -C %s for-each-ref --format='%%(refname) %%(objectname)' "
    "refs/heads refs/tags",
    repo);
  cr_assert_str_eq(rewritten_refs, expected);
  free(expected);

  char* actual =
    bbfg_test_read_command("git -C %s rev-parse 'HEAD^{tree}'", repo);
  cr_assert_str_eq(actual, expected_tree);
  free(actual);
  free(expected_tree);

  expected = bbfg_test_read_command(
    "git -C %s for-each-ref --format='%%(refname)' refs/heads refs/tags", repo);
  actual = bbfg_test_read_command("%s --list-rewrite-refs %s", bbfg, repo);
  cr_assert_str_eq(actual, expected);
  free(actual);
  free(expected);

  expected = bbfg_test_read_command(
    "git -C %s rev-list --topo-order --reverse --branches --tags", repo);
  actual = bbfg_test_read_command("%s --walk-rewrite-commits %s", bbfg, repo);
  cr_assert_str_eq(actual, expected);
  free(actual);
  free(expected);
  free(rewritten_refs);
  bbfg_test_cleanup(tmpdir);
}

Test(cli, rewrite_merge_refs)
{
  char tmpdir[BBFG_TEST_PATH_SIZE];
  char repo[BBFG_TEST_PATH_SIZE];
  const char* bbfg = bbfg_test_path();

  bbfg_test_init_repo(tmpdir, sizeof(tmpdir), repo, sizeof(repo));
  bbfg_test_add_merge_history(repo);
  bbfg_test_add_annotated_tag(repo);

  char* before_feature =
    bbfg_test_read_command("git -C %s rev-parse refs/heads/feature", repo);
  bbfg_test_strip_trailing_newline(before_feature);
  char* before_tag =
    bbfg_test_read_command("git -C %s rev-parse refs/tags/v1", repo);
  bbfg_test_strip_trailing_newline(before_tag);

  char* output = bbfg_test_read_command(
    "%s --rewrite-refs --delete dir/nested.txt --no-blob-protection %s",
    bbfg,
    repo);
  char* after_feature =
    bbfg_test_read_command("git -C %s rev-parse refs/heads/feature", repo);
  bbfg_test_strip_trailing_newline(after_feature);
  cr_assert_str_neq(after_feature, before_feature);
  char* after_tag =
    bbfg_test_read_command("git -C %s rev-parse refs/tags/v1", repo);
  bbfg_test_strip_trailing_newline(after_tag);
  cr_assert_str_neq(after_tag, before_tag);

  char* main_names = bbfg_test_read_command(
    "git -C %s ls-tree -r --name-only refs/heads/main", repo);
  cr_assert_null(strstr(main_names, "dir/nested.txt"));
  cr_assert_not_null(strstr(main_names, "feature.txt"));
  free(main_names);

  char* feature_names = bbfg_test_read_command(
    "git -C %s ls-tree -r --name-only refs/heads/feature", repo);
  cr_assert_null(strstr(feature_names, "dir/nested.txt"));
  cr_assert_not_null(strstr(feature_names, "feature.txt"));
  free(feature_names);

  char* tag_type =
    bbfg_test_read_command("git -C %s cat-file -t refs/tags/v1", repo);
  cr_assert_str_eq(tag_type, "tag\n");
  free(tag_type);

  cr_assert_eq(bbfg_test_run_command(
                 "git -C %s fsck --full --no-progress >/dev/null", repo),
               0);

  free(after_tag);
  free(before_tag);
  free(after_feature);
  free(before_feature);
  free(output);
  bbfg_test_cleanup(tmpdir);
}

Test(cli, delete_files_by_name)
{
  char tmpdir[BBFG_TEST_PATH_SIZE];
  char repo[BBFG_TEST_PATH_SIZE];
  const char* bbfg = bbfg_test_path();

  bbfg_test_init_repo(tmpdir, sizeof(tmpdir), repo, sizeof(repo));

  char* rewritten = bbfg_test_read_command(
    "%s --rewrite-ref refs/heads/main --delete-files nested.txt "
    "--no-blob-protection %s",
    bbfg,
    repo);
  bbfg_test_strip_trailing_newline(rewritten);
  cr_assert(strlen(rewritten) > 0);

  char* names =
    bbfg_test_read_command("git -C %s ls-tree -r --name-only HEAD", repo);
  cr_assert_null(strstr(names, "nested.txt"));
  cr_assert_not_null(strstr(names, "file.txt"));

  char* directories =
    bbfg_test_read_command("git -C %s ls-tree -d --name-only HEAD", repo);
  cr_assert_null(strstr(directories, "dir"));
  cr_assert_null(strstr(directories, "other"));
  free(directories);

  char* before_noop =
    bbfg_test_read_command("git -C %s rev-parse refs/heads/main", repo);
  bbfg_test_strip_trailing_newline(before_noop);
  char* noop = bbfg_test_read_command(
    "%s --rewrite-ref refs/heads/main --delete-files absent.txt "
    "--no-blob-protection %s",
    bbfg,
    repo);
  bbfg_test_strip_trailing_newline(noop);
  cr_assert_str_eq(noop, before_noop);

  free(noop);
  free(before_noop);
  free(rewritten);
  free(names);
  bbfg_test_cleanup(tmpdir);
}
