#!/bin/sh
# Prerequisites:
#  xcode
#  cmake 3.5

#####################
# This unzips the tar.gz and applies any patches stored in the google-test-source-patches directory
# uncompress_and_patch.bat
archive=${1}

unzip $archive

mv ${archive%.*} "google-test-source"

#cp google-test-source-patches/* google-test-source/

