#!/bin/bash

# go to batch files dirs
cd $(dirname "$0")

export DOC=$(osascript -e 'tell application "Xcode" 
	return path of document 1 whose name ends with (word -1 of (get name of window 1))
end tell')
export TARGET=$(osascript -e 'tell application "Xcode" 
	return name of active scheme of workspace document 1
end tell')
PLATFORM=$(osascript -e 'tell application "Xcode" 
	return platform of active run destination of workspace document 1
end tell')

PLATFORM=${PLATFORM/macosx/Mac}
PLATFORM=${PLATFORM/iphoneos/IOS}
export PLATFORM=${PLATFORM/tvos/TVOS}

echo Doc: ${DOC}
echo Target: ${TARGET}
echo Platform: ${PLATFORM}


osascript -e 'tell application "Terminal"
	activate
	set Dir to (system attribute "PWD")
	set Target to (system attribute "TARGET")
	set Platform to (system attribute "PLATFORM")
	set Doc to (system attribute "DOC")
	do script Dir & "/Build.sh " & Target & " Development " & Platform & " -singlefile=\"" & Doc & "\"; exit" 
end tell'
# ./Build.sh ${TARGET} Development ${PLATFORM} -singlefile="${DOC}"

# echo ${XCODEINFO}

# echo "-${ActivePath}-"

# osascript -e 'tell application "Xcode" to display dialog (system attribute "FILEPATH") buttons {"OK"} default button 1'
