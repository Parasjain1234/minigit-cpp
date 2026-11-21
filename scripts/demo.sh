#!/usr/bin/env bash
# End-to-end demonstration of the MiniGit command line.
# Usage: scripts/demo.sh [path-to-minigit]
set -euo pipefail

MINIGIT="${1:-minigit}"
workdir="$(mktemp -d)"
trap 'rm -rf "$workdir"' EXIT

bold() { printf '\n\033[1m%s\033[0m\n' "$*"; }

cd "$workdir"

bold "init"
"$MINIGIT" init

bold "add + commit"
printf 'alpha\nbeta\ngamma\n' > poem.txt
"$MINIGIT" add poem.txt
"$MINIGIT" commit -m "Add the first poem"

bold "branch + checkout"
"$MINIGIT" branch feature/verse
"$MINIGIT" checkout feature/verse

bold "diff"
printf 'alpha\nBETA\ngamma\ndelta\n' > poem.txt
"$MINIGIT" diff
"$MINIGIT" add poem.txt
"$MINIGIT" commit -m "Rewrite the second line"

bold "back to main and edit independently"
"$MINIGIT" checkout main
printf 'alpha\nbeta\ngamma\n' > poem.txt
printf 'new stanza\n' >> poem.txt
"$MINIGIT" add poem.txt
"$MINIGIT" commit -m "Append a stanza"

bold "merge (three-way)"
"$MINIGIT" merge feature/verse

bold "log --graph"
"$MINIGIT" log --graph

bold "status"
"$MINIGIT" status

bold "export"
"$MINIGIT" export | head -n 12

bold "done"
echo "Repository was created in $workdir and will be removed."
