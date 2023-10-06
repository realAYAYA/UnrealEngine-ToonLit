#!/usr/bin/env bash
# Copyright Epic Games, Inc. All Rights Reserved.

set -eu

UNAMEOS="$(uname -s)"
ADDRESS="127.0.0.1"
PORT="8000"
URL="http://$ADDRESS:$PORT/GPUDumpViewer.html"

SCRIPT=$(readlink -f "$0")
# Absolute path this script is in, thus /home/user/bin
SCRIPTPATH=$(dirname "$SCRIPT")

pushd "$SCRIPTPATH"


GetAllChildProcesses() {
	local Children=$(ps -o pid= ppid "$1")

	for PID in $Children
	do
		GetAllChildProcesses "$PID"
	done

	echo "$Children"
}

# Gather all the descendant children of this process, and first kill -TERM. If any child process
# is still alive finally send a -KILL
TermHandler() {
	MaxWait=30
	CurrentWait=0

	ProcessesToKill=$(GetAllChildProcesses $$)
	kill -s TERM $ProcessesToKill 2> /dev/null

	ProcessesStillAlive=$(ps -o pid= -p $ProcessesToKill)

	# Wait until all the processes have been gracefully killed, or max Wait time
	while [ -n "$ProcessesStillAlive" ] && [ "$CurrentWait" -lt "$MaxWait" ]
	do
		CurrentWait=$((CurrentWait + 1))
		sleep 1

		ProcessesStillAlive=$(ps -o pid= -p $ProcessesToKill)
	done

	# If some processes are still alive after MaxWait, lets just force kill them
	if [ -n "$ProcessesStillAlive" ]; then
		kill -s KILL $ProcessesStillAlive 2> /dev/null
	fi
}

# trap when SIGINT or SIGTERM are received with a custom function
trap TermHandler SIGTERM SIGINT

APPS=()
APPS+=(xdg-open)

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

ARGS+=("$URL")

echo "Executing:"
echo

echo "Starting simple webserver..."
exec python3 -m http.server "$PORT" --bind "$ADDRESS" &
P1=$!
sleep 1

echo "${ARGS[0]} \\"
for ((i=1; i < ${#ARGS[@]}; i++ )); do
	echo "  ${ARGS[$i]} \\";
done
echo

# Start the browser now that the server is running
"${ARGS[@]}"

# Wait on the webserver - in general this will be killed by a Ctrl-C
wait $P1

echo
echo "Closing ${CMD}..."

popd
