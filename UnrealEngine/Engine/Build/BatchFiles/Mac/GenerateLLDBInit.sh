#/bin/sh

if [ ! -f ~/.lldbinit ]; then
  echo 'echo -n "settings set target.inline-breakpoint-strategy always\n"' | bash > ~/.lldbinit
  echo 'echo -n "command script import \"`pwd`/../../../Extras/LLDBDataFormatters/UEDataFormatters_2ByteChars.py\"\n"' | bash >> ~/.lldbinit
fi
