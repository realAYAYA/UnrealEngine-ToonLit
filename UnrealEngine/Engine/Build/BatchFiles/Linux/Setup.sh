#!/bin/bash

SCRIPT_DIR=$(cd "$(dirname "$BASH_SOURCE")" ; pwd)
TOP_DIR=$(cd "$SCRIPT_DIR/../../.." ; pwd)

# args: package
# returns 0 if installed, 1 if it needs to be installed
PackageIsInstalled()
{
    PackageName=$1

    dpkg-query -W -f='${Status}\n' $PackageName | head -n 1 | awk '{print $3;}' | grep -q '^installed$'
}

# main
set -e

TOP_DIR=$(cd "$SCRIPT_DIR/../../.." ; pwd)
cd "${TOP_DIR}"

if [ -e /etc/os-release ]; then
  source /etc/os-release
  # Ubuntu/Debian/Mint
  if [[ "$ID" == "ubuntu" ]] || [[ "$ID_LIKE" == "ubuntu" ]] || [[ "$ID" == "debian" ]] || [[ "$ID_LIKE" == "debian" ]] || [[ "$ID" == "tanglu" ]] || [[ "$ID_LIKE" == "tanglu" ]]; then

    # Both mono and the compiler toolchain are now bundled.
    # build-essential can still be useful for making sure the system has make
    DEPS="
      build-essential
      "

    for DEP in $DEPS; do
      if ! PackageIsInstalled $DEP; then
        echo "Attempting installation of missing package: $DEP"
        set -x
        sudo apt-get install -y $DEP
        set +x
      fi
    done
  fi
fi

# Install our bundled toolchain unless we are running a Perforce build or a an our of source toolchain is available.
if [ ! -f Build/PerforceBuild.txt ] && [ -z "$LINUX_MULTIARCH_ROOT" ] && [ -z "$UE_SDKS_ROOT" ]; then
  echo "Installing a bundled clang toolchain"
  pushd Build/BatchFiles/Linux > /dev/null
  ./SetupToolchain.sh
  popd > /dev/null
fi

# Provide the hooks for locally building third party libs if needed
echo
pushd Build/BatchFiles/Linux > /dev/null
./BuildThirdParty.sh
popd > /dev/null

echo "Setup successful."
touch Build/OneTimeSetupPerformed
