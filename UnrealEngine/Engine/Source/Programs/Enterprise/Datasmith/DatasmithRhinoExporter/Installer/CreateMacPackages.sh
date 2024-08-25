#! /bin/sh

set -e
set -x

PLUGIN_NAME=$1
PLUGIN_VERSION=$2
SCRIPT_DIR=$(builtin cd $(dirname $0); pwd)
PLUGIN_BASE_DIR="$(dirname $SCRIPT_DIR)"
ENGINE_DIR="$PLUGIN_BASE_DIR/../../../../.."
TBB_BINARY_DIR="$ENGINE_DIR/Binaries/ThirdParty/Intel/TBB/Mac"
PLUGIN_BINARY_DIR="$ENGINE_DIR/Binaries/Mac/Rhino/$PLUGIN_VERSION"
DATASMITH_FACADE_BINARY_DIR="$ENGINE_DIR/Binaries/Mac/DatasmithFacadeCSharp"
PLUGIN_RHP_PATH="$PLUGIN_BINARY_DIR/$PLUGIN_NAME.rhp"
PLUGIN_MACRHI_PATH="$PLUGIN_BINARY_DIR/$PLUGIN_NAME.macrhi"

echo "Removing existing .rhp and .macrhi files."
rm -rf "$PLUGIN_RHP_PATH"
rm -f "$PLUGIN_MACRHI_PATH"

echo "Creating .rhp package"
mkdir "$PLUGIN_RHP_PATH"
cp "$DATASMITH_FACADE_BINARY_DIR/"*".dylib" "$PLUGIN_RHP_PATH"
cp "$TBB_BINARY_DIR/"*".dylib" "$PLUGIN_RHP_PATH"
cp "$PLUGIN_BINARY_DIR/$PLUGIN_NAME.dll" "$PLUGIN_RHP_PATH/$PLUGIN_NAME.rhp"
cp "$PLUGIN_BASE_DIR/Config/DatasmithRhino.rhp.config" "$PLUGIN_RHP_PATH/$PLUGIN_NAME.rhp.config"
# Starting Rhino 8, Rhino UI systems on Mac supports same .rui file for UI library as on Windows
cp "$PLUGIN_BASE_DIR/SharedResources/DatasmithRhino.rui" "$PLUGIN_RHP_PATH/$PLUGIN_NAME.rui"

mkdir "$PLUGIN_RHP_PATH/Resources"
cp "$PLUGIN_BASE_DIR/SharedResources/DatasmithRhino.plist" "$PLUGIN_RHP_PATH/Resources/DatasmithRhino.plist"

echo "Adding localized cultures to .rhp package"
CULTURE_LIST=("de" "ko" "es" "fr" "ja" "pt" "zh")
for CULTURE in "${CULTURE_LIST[@]}"
do
    #add_language(culture_name, culture_code)
    CULTURE_TARGET_PATH=$PLUGIN_RHP_PATH/$CULTURE
    mkdir "$CULTURE_TARGET_PATH"
    cp "$PLUGIN_BINARY_DIR/$CULTURE/"*".dll" "$CULTURE_TARGET_PATH"
done

echo "Generating $PLUGIN_MACRHI_PATH"
ditto -c -k --keepParent "$PLUGIN_RHP_PATH" "$PLUGIN_MACRHI_PATH"