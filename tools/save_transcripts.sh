#!/usr/bin/env bash
# Copies this project's raw Claude Code session transcripts (.jsonl) into
# transcripts/ byte-for-byte, verifies each copy against its source, and
# records SHA-256 checksums. Nothing is edited or filtered.
#
# Run it after the last message of a session: a session's file keeps growing
# while the session is open, and re-running replaces the copy with the
# complete file. The two older transcripts at the repo root are left untouched.
set -euo pipefail
cd "$(dirname "$0")/.."

src=${CLAUDE_TRANSCRIPT_DIR:-$HOME/.claude/projects/$(pwd | tr '/' '-')}
dst=transcripts
[[ -d $src ]] || { echo "save_transcripts: no transcript dir $src" >&2; exit 1; }
mkdir -p "$dst"

shopt -s nullglob
files=("$src"/*.jsonl)
(( ${#files[@]} )) || { echo "save_transcripts: no .jsonl in $src" >&2; exit 1; }

for f in "${files[@]}"; do
    cp -p "$f" "$dst/"
    cmp -s "$f" "$dst/$(basename "$f")" || { echo "copy mismatch: $f" >&2; exit 1; }
    echo "saved $(basename "$f") ($(stat -c %s "$f") bytes)"
done
(cd "$dst" && sha256sum -- *.jsonl > SHA256SUMS)
echo "checksums: $dst/SHA256SUMS"
