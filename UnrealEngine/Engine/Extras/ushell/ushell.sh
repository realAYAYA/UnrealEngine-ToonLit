#!/bin/bash

# Check whether this script has been "sourced" into a host shell
if [ $0 = "$BASH_SOURCE" ]; then
    # The script was not sourced, so source it in a child shell and propagate
    # any arguments
    args=""
    for arg in "$@"
    do
        escaped=$(printf '%q' "$arg")
        args="$args $escaped"
    done
    bash --init-file <(echo "source \"$HOME/.bashrc\"; source \"$BASH_SOURCE\" $args")
    exit $?
fi

# Determine the host shell
if [ -n "$BASH" ]; then
    host_shell=bash
elif [ -n "$ZSH_NAME" ]; then
    host_shell=zsh
elif [ -n "$SHELL" ]; then
    host_shell=$(basename $SHELL)
fi

if [ -z $host_shell ]; then
    echo Error: Unable to determine host shell because \$SHELL is unset.
    echo
    return
fi

# Start the session up
if [ "$WINDIR" ]; then
    channel=nt
    cookie=$(cygpath --windows --absolute /tmp/ushell_$$_shell_cookie)
else
    channel=posix
    cookie=/tmp/ushell_$$_shell_cookie
fi

$(dirname ${BASH_SOURCE:-$0})/channels/flow/$channel/boot.sh --bootarg=$host_shell,$cookie "$@"

if [ -f $cookie ]; then
    chmod u+x $cookie
    source $cookie
    unlink $cookie
    unset cookie
fi
