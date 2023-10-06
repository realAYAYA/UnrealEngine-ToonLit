#!/bin/bash
# Copyright Epic Games, Inc. All Rights Reserved.

set -eu
set -o pipefail

read -ra keeparches <<< "$(lipo -archs "$2" | sort)"
echo "Source binary $2 has arches: ${keeparches[*]}"

function thin {
	binaryname=$(basename "$1")
	extension=${binaryname##*.}
	# check for .dylib and files with no extension at all (CEF is a dylib without an extension)
	if [[ "${extension}" == "${binaryname}" || "${extension}" == "dylib" ]]; then
		# use file to check if it's a dylib,
		if [ "$(file -b "$1" | grep "dynamically linked shared library")" != "" ]; then
			read -ra arches <<< "$(lipo -archs "$1" | sort)"

			if [ "${arches[*]}" != "${keeparches[*]}" ]; then
				echo "Thinning $1 down to ${keeparches[*]}"
				mv "$1" "$1.tmp"

				archparams=(${keeparches[@]/#/"--arch "})
				ditto "${archparams[@]}" "$1.tmp" "$1"
				rm "$1.tmp"
				
				# now check that we have all the architectures we need, otherwise the app won't work with this dylib
				read -ra arches <<< "$(lipo -archs "$1" | sort)"
				if [ "${arches[*]}" != "${keeparches[*]}" ]; then
					echo "Dylib $1 did not have all the architectures needed (had '${arches[*]}', needed '${keeparches[*]}')"
					exit 1
				fi
			fi
		fi
	fi
}

find "$1" | while read -r file; do thin "$file"; done
