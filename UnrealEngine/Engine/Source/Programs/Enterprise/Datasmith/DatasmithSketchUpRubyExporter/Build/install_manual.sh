# ls "$HOME/Library/Application Support/SketchUp 2020/SketchUp/Plugins"
set -e
cp -rv `dirname "$0"`/../.build/2020/Plugin/. "$HOME/Library/Application Support/SketchUp 2020/SketchUp/Plugins"
