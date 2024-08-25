#!/bin/bash

# Copyright Epic Games, Inc. All Rights Reserved.

_error()   { printf "\x1b[91m!! ERROR: %s\x1b[0m\n" "$1" ; }
_header()  { printf "\x1b[96m-- %s\x1b[0m\n" "$1" ; }
_success() { printf "\x1b[92m%s\x1b[0m\n" "$1" ; }

py_ver_maj=3
if [ $(uname) == "Linux" ]; then
    py_ver_min=11
else
    py_ver_min=12
fi

py_ver=$py_ver_maj.$py_ver_min

# If we've already a destination we're done here
if [ -z "$1" ]; then
    _error "Missing 'working dir' argument"
    exit 1
fi

py_mark=$py_ver.version

# If current/ is already at the correct version, we're done
if [ -e $1/python/current/$py_mark ]; then
    exit 0
fi

dest_dir=$1/python/$py_ver
current_dir=$(dirname $dest_dir)/current
function _link_dest_current()
{
    _header "Symlinking $dest_dir to $current_dir"
    ln -s $dest_dir $current_dir
    touch $dest_dir/$py_mark
}

# Remove any lingering current/
if [ -d $current_dir ]; then
    if ! rm $current_dir; then
        _error "Failed unlinking $current_dir"
        exit 1
    fi
fi

# If we've already a destination we're alomst done here
if [ -e $dest_dir/$py_mark ]; then
    _link_dest_current
    exit 0
fi

# Remove any lingering prior venvs
if [ -d $dest_dir ]; then
    if ! rm -rf $dest_dir; then
        _error "Failed removing $dest_dir"
        exit 1
    fi
fi

# Check the versions good enough
function _check_version()
{
    # Bash's here document syntax is ugly
    cat << EOF | $1 -EsSB
import sys
vi = sys.version_info
raise SystemExit(vi.major != ${py_ver_maj} or vi.minor != ${py_ver_min})
EOF
    return $?
}

function _bin_select()
{
    py_bin=
    for candidate in "$@"; do
        if ! command -v $candidate; then
            continue
        fi

        if ! _check_version $candidate; then
            continue
        fi

        py_bin=$candidate
        return 0
    done
    return 1
}

_header "Looking for a $py_ver Python binary"
_bin_select \
    $USHELL_PY_BIN \
    python$py_ver_maj \
    /usr/local/opt/python@${py_ver_maj}.*/bin/python$py_ver_maj \
    /usr/bin/python$py_ver_maj.*[0-9] \
    /usr/local/bin/python$py_ver_maj.*[0-9] \
    python

if [ -z "$py_bin" ]; then
    _error "No suitable binary found that is $py_ver"
    echo ""
    echo "Examples to install Python $py_ver:"
    echo ""
    echo "   Ubuntu: apt install python$py_ver"
    echo "      Mac: brew install python@$py_ver"
    echo ""
    exit 1
fi

_success "Found: $py_bin: (as $($py_bin --version))"

# Create the Python virtual env. Ubuntu doesn't come with the ensurepip module
# which venv will run on start up. So our venv is created without Pip initially.
temp_dir=$1/python/${py_ver}_$$

_header "Creating a virtual environment in $temp_dir"
if ! $py_bin -m venv --without-pip $temp_dir; then
    _error "venv-based virtual environment creation failed"
    exit 1
fi

# Verify the virtualenv is the version we expect
_check_version $temp_dir/bin/python
if [ "$?" -ne "0" ]; then
    _error "Virtualenv is an unexpected version"
    exit 1
fi

# Make sure the venv has Pip and CA certificates
_header "Fetching Pip"
curl -L https://bootstrap.pypa.io/get-pip.py | $temp_dir/bin/python
$temp_dir/bin/python -m pip install certifi

# Swap the temp dir into place. If we fail someone else did it first
if mv $temp_dir $dest_dir; then
    _link_dest_current
else
    rm -rf $temp_dir
fi

_success Done!
echo
echo
