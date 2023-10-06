#!/bin/bash

# Copyright Epic Games, Inc. All Rights Reserved.

_error()   { printf "\x1b[91m!! ERROR: %s\x1b[0m\n" "$1" ; }
_header()  { printf "\x1b[96m-- %s\x1b[0m\n" "$1" ; }
_success() { printf "\x1b[92m%s\x1b[0m\n" "$1" ; }

py_ver=3.10

# Try and find a suitable Python binary
_header "Looking for a Python binary"
if command -v python${py_ver}; then
    py_bin=python${py_ver}
elif command -v /usr/local/opt/python@${py_ver}/bin/python3; then
    py_bin=/usr/local/opt/python@${py_ver}/bin/python3
elif command -v python3; then
    py_bin=python3
else
    echo FAILED
    exit 1
fi
_success Found

# Work out what version of Python we've got
py_ver_full=$($py_bin --version)
IFS=" "
read -ra py_ver_full <<< "$py_ver_full"
py_ver_full=${py_ver_full[1]}

# Check the versions good enough
function _check_version()
{
    # Bash's here document syntax is ugly
    cat << EOF | $1
l = lambda x: [int(y) for y in x.split('.')][0:2]
raise SystemExit(l('${py_ver_full}') < l('${py_ver}'))
EOF
    return $?
}

_check_version $py_bin
if [ "$?" -ne "0" ]; then
    _error "Version $py_ver_full is too old. $py_ver is required"
    echo "Examples to install Python $py_ver:"
    echo "   Ubuntu: apt install python$py_ver"
    echo "      Mac: brew install python@$py_ver"
    exit 1
fi
echo Using version $py_ver_full

# Check if our destination exists already
dest_dir=$1/python/$py_ver_full
if [ -d "$dest_dir" ]; then
    exit 0
fi

# Make sure that the Python install has Pip installed
_header "Checking for Pip"
curl -L https://bootstrap.pypa.io/get-pip.py | $py_bin
if [ "$?" -ne "0" ]; then
    _error "Failed to ensure Pip is installed"
    exit 1
fi

# Create the Python virtual env
_header "Fetching virtual env"

temp_dir=$1/python/${py_ver_full}_$$

if ! $py_bin -m pip install virtualenv; then
    _error "Unable to install 'virtualenv' Pip package"
    _error "Please ensure Pip is installed"
    exit 1
fi

_header "Creating a virtualenv in $temp_dir"
if ! $py_bin -m virtualenv $temp_dir; then
    _error "Virtualenv creation failed"
    exit 1
fi

# Verify the virtualenv is the version we expect
_check_version $temp_dir/bin/python
if [ "$?" -ne "0" ]; then
    _error "Virtualenv is an unexpected version"
    exit 1
fi

# Swap the temp dir into place. If we fail someone else did it first
if mv $temp_dir $dest_dir; then
    current_dir=$(dirname $dest_dir)/current
    _header "Symlinking $dest_dir to $current_dir"
    rm $current_dir 2>/dev/null
    ln -s $dest_dir $current_dir
else
    rm -rf $temp_dir
fi

_success Done!
echo
echo
