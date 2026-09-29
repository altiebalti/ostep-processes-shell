#!/usr/bin/env bash
# Build the dev image (if needed) and run a command inside it.
#   ./dev.sh            -> interactive shell in /work/processes-shell
#   ./dev.sh make test  -> build + run the test suite, then exit
set -euo pipefail
cd "$(dirname "$0")"

docker build -q -t wish-dev . >/dev/null

# Only allocate a TTY when we actually have one (so CI / piped use works).
tty_flags=(-i)
[[ -t 0 && -t 1 ]] && tty_flags+=(-t)

if [[ $# -eq 0 ]]; then
    exec docker run --rm "${tty_flags[@]}" -v "$PWD:/work" wish-dev
else
    exec docker run --rm "${tty_flags[@]}" -v "$PWD:/work" wish-dev bash -lc "$*"
fi
