#!/bin/bash
# Copyright Epic Games, Inc. All Rights Reserved.

# Fixes for case sensitive filesystem.
echo "Fixing inconsistent case in filenames."
for BASE in Content/Editor/Slate Content/Slate Documentation/Source/Shared/Icons; do
	if [ ! -d $BASE ]; then
		continue;
	fi
	find $BASE -name "*.PNG" | while read PNG_UPPER; do
		png_lower="$(echo "$PNG_UPPER" | sed 's/.PNG$/.png/')"
		if [ ! -f "$png_lower" ]; then
			PNG_UPPER=$(basename "$PNG_UPPER")
			echo "$png_lower -> $PNG_UPPER"
			# link, and not move, to make it usable with Perforce workspaces
			ln -sf "`basename "$PNG_UPPER"`" "$png_lower"
		fi
	done
done
