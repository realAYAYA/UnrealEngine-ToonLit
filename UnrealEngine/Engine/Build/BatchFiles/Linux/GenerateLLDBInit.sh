#!/bin/bash

if [ ! -f ~/.lldbinit ]; then
  echo -ne "settings set target.inline-breakpoint-strategy always\n" > ~/.lldbinit
  echo -ne "command script import \"`pwd`/../../../Extras/LLDBDataFormatters/UEDataFormatters_2ByteChars.py\"\n" >> ~/.lldbinit
fi
