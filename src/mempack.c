#include "mempack.h"

#include <git2/odb_backend.h>
#include <git2/pack.h>
#include <git2/sys/mempack.h>
#include <git2/sys/odb_backend.h>

#ifndef BBFG_LIBGIT2_HAS_THIN_PACK
#define BBFG_LIBGIT2_HAS_THIN_PACK 0
#endif

static int
write_pack(git_odb* odb, const git_buf* pack)
{
  git_odb_writepack* writepack = NULL;
  if (git_odb_write_pack(&writepack, odb, NULL, NULL) < 0) {
    return -1;
  }

  git_indexer_progress stats = { 0 };
  int result = writepack->append(writepack, pack->ptr, pack->size, &stats);
  if (result == 0) {
    result = writepack->commit(writepack, &stats);
  }

  writepack->free(writepack);
  return result;
}

int
bbfg_mempack_begin(BbfgMempack* mempack, git_repository* repo)
{
  *mempack = (BbfgMempack)BBFG_MEMPACK_INIT;

  if (git_repository_odb(&mempack->odb, repo) < 0 ||
      git_mempack_new(&mempack->backend) < 0) {
    bbfg_mempack_dispose(mempack);
    return -1;
  }

  if (git_odb_add_backend(mempack->odb, mempack->backend, 999) < 0) {
    mempack->backend->free(mempack->backend);
    mempack->backend = NULL;
    bbfg_mempack_dispose(mempack);
    return -1;
  }

  return 0;
}

#if !BBFG_LIBGIT2_HAS_THIN_PACK
static int
write_tag_pack(git_odb* odb,
               git_repository* repo,
               const BbfgOidMap* rewritten_tags)
{
  if (rewritten_tags == NULL || rewritten_tags->count == 0) {
    return 0;
  }

  git_packbuilder* packbuilder = NULL;
  if (git_packbuilder_new(&packbuilder, repo) < 0) {
    return -1;
  }

  git_packbuilder_set_threads(packbuilder, 0);
  int result = 0;
  size_t i;
  for (i = 0; i < rewritten_tags->count; i++) {
    const git_oid* tag_id = bbfg_oid_map_value_at(rewritten_tags, i);
    result = git_packbuilder_insert(packbuilder, tag_id, NULL);
    if (result < 0) {
      break;
    }
  }

  git_buf pack = GIT_BUF_INIT;
  if (result == 0) {
    result = git_packbuilder_write_buf(&pack, packbuilder);
  }
  if (result == 0) {
    result = write_pack(odb, &pack);
  }

  git_buf_dispose(&pack);
  git_packbuilder_free(packbuilder);
  return result;
}
#endif

int
bbfg_mempack_commit(BbfgMempack* mempack,
                    git_repository* repo,
                    const BbfgOidMap* rewritten_tags)
{
#if BBFG_LIBGIT2_HAS_THIN_PACK
  (void)rewritten_tags;
  git_packbuilder* packbuilder = NULL;
  if (git_packbuilder_new(&packbuilder, repo) < 0) {
    return -1;
  }

  git_packbuilder_set_threads(packbuilder, 0);

  git_buf pack = GIT_BUF_INIT;
  int result = git_mempack_write_thin_pack(mempack->backend, packbuilder);
  if (result == 0) {
    result = git_packbuilder_write_buf(&pack, packbuilder);
  }
#else
  git_buf pack = GIT_BUF_INIT;
  int result = git_mempack_dump(&pack, repo, mempack->backend);
#endif
  if (result == 0) {
    result = write_pack(mempack->odb, &pack);
  }
#if !BBFG_LIBGIT2_HAS_THIN_PACK
  if (result == 0) {
    result = write_tag_pack(mempack->odb, repo, rewritten_tags);
  }
#endif

  git_buf_dispose(&pack);
#if BBFG_LIBGIT2_HAS_THIN_PACK
  git_packbuilder_free(packbuilder);
#endif
  if (result == 0) {
    result = git_mempack_reset(mempack->backend);
  }

  return result;
}

void
bbfg_mempack_abort(BbfgMempack* mempack)
{
  if (mempack->backend != NULL) {
    (void)git_mempack_reset(mempack->backend);
  }
}

void
bbfg_mempack_dispose(BbfgMempack* mempack)
{
  git_odb_free(mempack->odb);
  mempack->odb = NULL;
  mempack->backend = NULL;
}
