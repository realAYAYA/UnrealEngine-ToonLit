#!/bin/sh

# Check for case-sensitive file system
rm -f casetest*
touch casetestABC
touch casetestabc
if [ $(ls casetest* | wc -l) -gt 1 ]; then
    # Case sensitive filesystem.
    for BASE in Content/Editor/Slate Content/Slate Documentation/Source/Shared/Icons; do
        find ../../../$BASE -name "*PNG" | while read PNG_UPPER; do 
            png_lower="$(echo "$PNG_UPPER" | sed 's/PNG$/png/')"
            echo "$PNG_UPPER -> $png_lower"
            mv "$PNG_UPPER" "$png_lower"
        done
    done
fi
rm -f casetest*

# Copy UnrealEditorServices to ~/Library/Services
if [ ! -d ~/Library/Services/UnrealEditorServices.app ]; then
	if [ -d ../../../Binaries/Mac/UnrealEditorServices.app ]; then
		cp -r ../../../Binaries/Mac/UnrealEditorServices.app ~/Library/Services/UnrealEditorServices.app
	fi
fi
