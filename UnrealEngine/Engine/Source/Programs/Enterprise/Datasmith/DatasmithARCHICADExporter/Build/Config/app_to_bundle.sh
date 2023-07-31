#!/bin/sh

ConfigPath=`dirname "$0"`
projectPath=$ConfigPath/..

RelativeEnginePath=$projectPath/../../../../../../../Engine
EnginePath=`python -c "import os; print(os.path.realpath('$RelativeEnginePath'))"`

echo "Renaming *.app to *.bundle"
pushd $EnginePath/Binaries/Mac/DatasmithARCHICADExporter
mv DatasmithARCHICAD23Exporter.app DatasmithARCHICAD23Exporter.bundle
mv DatasmithARCHICAD24Exporter.app DatasmithARCHICAD24Exporter.bundle
mv DatasmithARCHICAD25Exporter.app DatasmithARCHICAD25Exporter.bundle
mv DatasmithARCHICAD26Exporter.app DatasmithARCHICAD26Exporter.bundle
popd
