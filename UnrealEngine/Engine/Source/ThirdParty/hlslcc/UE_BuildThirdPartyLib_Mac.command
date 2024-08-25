#!/bin/sh

pushd hlslcc

p4 edit $THIRD_PARTY_CHANGELIST lib/Mac/...

# compile Mac
pushd projects/XCode
xcodebuild -configuration Release clean
xcodebuild -configuration Debug clean
xcodebuild -configuration Release 
xcodebuild -configuration Debug 
popd

popd

echo 
echo Remember to recompile for other platforms too!