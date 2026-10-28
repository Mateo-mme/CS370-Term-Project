# Claude Code transcripts

These are the raw, unmodified `.jsonl` session transcripts, preserved for
milestone submission as the course requires.

| File | Session |
|------|---------|
| `../1f14fe9d-1da8-4f97-a61b-2e1d8f086955.jsonl` | earlier session (uploaded before Milestone 1 work; left at the repo root, untouched) |
| `../f8d154be-8d11-48d0-ab70-6ec5325f30e9.jsonl` | earlier session (as above) |
| `bab1c9c8-972d-49a5-86b8-243c8fa82bf4.jsonl` | Milestone 1 implementation session (Phases 0–6) |

`tools/save_transcripts.sh` (or `make transcripts`) copies the files from
`~/.claude/projects/<project>/` byte-for-byte, checks each copy against its
source with `cmp`, and writes `SHA256SUMS`. Verify with:

```sh
cd transcripts && sha256sum -c SHA256SUMS
```

An open session's transcript keeps growing, so run the script after the
session's last message, then commit.
