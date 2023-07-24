#!/usr/bin/env bash
# Copyright Epic Games, Inc. All Rights Reserved.

set -eu

UNAMEOS="$(uname -s)"

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" ; pwd)
CHROME_USER_DATA=$(mktemp -d -t GPUDumpViewerUserData-XXXXXX)

APPS=()
APPS+=(google-chrome-stable)
APPS+=(google-chrome)

if [[ "${UNAMEOS}" =~ "Darwin" ]]; then
	APPS+=(open)
fi

CMD=
for val in "${APPS[@]}"; do
	CMD="$(command -v "${val}")" || true
	if [[ -n "${CMD}" ]]; then
		break
	fi
done

if [[ -z "${CMD}" ]]; then
	echo "ERROR: Browser launch command not found"
	exit 1
fi

ARGS=("${CMD}")

if [[ "${CMD}" == *open ]]; then
	ARGS+=(-a "google chrome")
	ARGS+=(--new)
	ARGS+=(-W)
fi

ARGS+=("file://${SCRIPT_DIR}/GPUDumpViewer.html")

if [[ "${CMD}" == *open ]]; then
	ARGS+=(--args)
fi

# --allow-file-access-from-files allow to load a file from a file:// webpage required for GPUDumpViewer.html to work.
# --user-data-dir is required to force chrome to open a new instance so that --allow-file-access-from-files is honored.
ARGS+=(--allow-file-access-from-files)
ARGS+=(--new-window)
ARGS+=(--incognito)
ARGS+=("--user-data-dir=${CHROME_USER_DATA}")

echo "Executing:"
echo

echo "${ARGS[0]} \\"
for ((i=1; i < ${#ARGS[@]}; i++ )); do
	echo "  ${ARGS[$i]} \\";
done
echo

"${ARGS[@]}"

echo
echo "Closing ${CMD}..."

if [[ -n "${CHROME_USER_DATA}" ]]; then
	# Wait for 2s to shut down so that CHROME_USER_DATA can be deleted completely
	sleep 2
	rm -rf "${CHROME_USER_DATA}"
fi
