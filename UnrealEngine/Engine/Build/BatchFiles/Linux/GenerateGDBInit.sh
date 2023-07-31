#!/bin/bash
# Copyright Epic Games, Inc. All Rights Reserved.

SCRIPT_DIR=$(cd "$(dirname "$BASH_SOURCE")" ; pwd)
TOP_DIR=$(cd "$SCRIPT_DIR/../../.." ; pwd)

AddGDBPrettyPrinters()
{
	echo -ne "Attempting to set up UE pretty printers for gdb (existing UEPrinters.py, if any, will be overwritten)...\n\t"

	# Copy the pretty printer into the appropriate folder.
	mkdir -p ~/.config/Epic/GDBPrinters/
	if [ -e ~/.config/Epic/GDBPrinters/UEPrinters.py ]; then
		chmod 644 ~/.config/Epic/GDBPrinters/UEPrinters.py 	# set R/W so we can overwrite it
	fi
	cp "$TOP_DIR/Extras/GDBPrinters/UEPrinters.py" ~/.config/Epic/GDBPrinters/
	echo -ne "updated UEPrinters.py\n\t"
	chmod 644 ~/.config/Epic/GDBPrinters/UEPrinters.py 	# set R/W again (it can be read-only if copied from Perforce)

	# Check if .gdbinit exists. If not create else add needed parts.
	if [ ! -f ~/.gdbinit ]; then
		echo "no ~/.gdbinit file found - creating a new one."
		echo -e "python \nimport sys\n\nsys.path.append('$HOME/.config/Epic/GDBPrinters/')\n\nfrom UEPrinters import register_ue_printers\nregister_ue_printers(None)\nprint(\"Registered pretty printers for UE classes\")\n\nend" >> ~/.gdbinit
	else
		if grep -q "register_ue_printers" ~/.gdbinit; then
			echo "found necessary entries in ~/.gdbinit file, not changing it."
		else
			echo -e "cannot modify .gdbinit. Please add the below lines manually:\n\n"
			echo -e "python"
			echo -e "\timport sys"
			echo -e "\tsys.path.append('$HOME/.config/Epic/GDBPrinters/')"
			echo -e "\tfrom UEPrinters import register_ue_printers"
			echo -e "\tregister_ue_printers(None)"
			echo -e "\tprint(\"Registered pretty printers for UE classes\")"
			echo -e "\tend"
			echo -e "\n\n"
		fi
	fi
}

# Add GDB scripts for common Unreal types.
AddGDBPrettyPrinters
