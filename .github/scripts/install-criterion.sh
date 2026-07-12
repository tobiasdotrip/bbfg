#!/usr/bin/env bash
set -euo pipefail

: "${CI_CRITERION_PREFIX:?CI_CRITERION_PREFIX must be set}"

criterion_version="${CRITERION_VERSION:-v2.4.3}"
prefix="$CI_CRITERION_PREFIX"
if [[ "$prefix" != /* ]]; then
  prefix="$PWD/$prefix"
fi

criterion_pc="$(find "$prefix" -path '*/pkgconfig/criterion.pc' -print -quit 2>/dev/null || true)"
if [[ -z "$criterion_pc" ]]; then
  source_dir="${TMPDIR:-/tmp}/criterion-${criterion_version#v}"
  rm -rf "$source_dir" "$prefix"
  mkdir -p "$(dirname "$prefix")"

  git clone --depth 1 --branch "$criterion_version" \
    https://github.com/Snaipe/Criterion.git "$source_dir"
  criterion_meson_env=()
  if [[ -n "${CRITERION_CFLAGS:-}" ]]; then
    criterion_meson_env+=(CFLAGS="$CRITERION_CFLAGS")
  fi
  env "${criterion_meson_env[@]}" meson setup "$source_dir/build" "$source_dir" \
    --buildtype=release \
    --prefix="$prefix" \
    -Dcxx-support=disabled \
    -Dsamples=false \
    -Dtests=false
  meson compile -C "$source_dir/build"
  meson install -C "$source_dir/build"

  criterion_pc="$(find "$prefix" -path '*/pkgconfig/criterion.pc' -print -quit)"
fi

pkgconfig_dir="$(dirname "$criterion_pc")"
criterion_lib_dir="$(dirname "$pkgconfig_dir")"
export PKG_CONFIG_PATH="$pkgconfig_dir${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
export PATH="$prefix/bin:$PATH"
export LD_LIBRARY_PATH="$prefix/lib:$prefix/lib64:$criterion_lib_dir${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

pkg-config --exists criterion
