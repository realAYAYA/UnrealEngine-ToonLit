#!/bin/sh

# This script is called by UE tools to source environment variables from the users .unrealrc script(s)
# It expects to be called from the engine directory and the order is ~/.unrealrc then <workspace>/.unrealrc

# To setup the environment and dotnet, use SetupEnvironment.sh -dotnet Engine/Build/BatchFiles/Mac
# (It's not clear why we can't just determine that path outselves, but for legacy reasons we'll keep it
# as a param)

if [ -f ~/.unrealrc ]; then
    source ~/.unrealrc
fi

if [ -f .unrealrc ]; then
    source .unrealrc
fi

while test $# -gt 0
do
    case "$1" in
        -dotnet) 
            # echo "setting up dotnet"
            source "$2/SetupDotnet.sh" "$2"
            shift
            ;;
        -*) echo "bad argument $1"
            ;;
    esac
    shift
done
