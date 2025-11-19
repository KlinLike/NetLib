#!/usr/bin/env bash
set -e
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR"

do_build() { make reactor-test; }
do_run() { echo "Echo server starting on port 8888"; echo "Test with: python3 tests/echo_client.py <IP> 8888 \"Hello Reactor\""; ./build/reactor_test 8888; }
do_clean() { make clean; }

if [ $# -gt 0 ]; then
  case "$1" in
    build) do_build ;;
    run) do_run ;;
    clean) do_clean ;;
    *) echo "Commands: build | run | clean"; exit 1 ;;
  esac
  exit 0
fi

while true; do
  echo "Commands: build | run | clean | quit"
  read -r -p "> " cmd
  case "$cmd" in
    build|b) do_build ;;
    run|r) do_run ;;
    clean|c) do_clean ;;
    quit|q|exit) exit 0 ;;
    *) echo "Unknown command" ;;
  esac
done