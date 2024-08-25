#!/system/bin/sh
HERE="$(cd "$(dirname "$0")" && pwd)"
export UE_INTERNAL_FOLDER="/data/user/0/${PACKAGE_NAME}/files/"
export LD_PRELOAD="$HERE/libScudoMemoryTrace.so"
"$@"
