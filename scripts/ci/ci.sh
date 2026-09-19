#!/bin/sh

set -eu

ROOT=$(CDPATH= cd "$(dirname "$0")/../.." && pwd)

if ! command -v xmake >/dev/null 2>&1; then
    echo "\`xmake\` not found in PATH" >&2
    exit 1
fi

case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        PLATFORM=windows
        ;;
    Darwin)
        PLATFORM=macosx
        ;;
    *)
        PLATFORM=linux
        ;;
esac

cd "$ROOT"
trap 'xmake f -m release -p "$PLATFORM" -y >/dev/null 2>&1 || true' EXIT

for config in debug release; do
    echo "======> Configure $config"
    xmake f -m "$config" -p "$PLATFORM" -y
    echo "======> Build"
    xmake build
    echo "======> Test"
    xmake test
done

echo "======> Configure release (static CRT)"
xmake f -m release -p "$PLATFORM" --runtimes=MT -y

echo "======> Build"
xmake build

echo "======> Test"
xmake test