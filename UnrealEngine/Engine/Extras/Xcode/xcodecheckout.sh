#!/bin/bash


# This script can be specified in Xcode's behaviors to checkout files that are locked for editing.
# It assumes the P4 commandline tools are installed, and the active p4config points to a workspace
# that maps to the path of the file

 /usr/local/bin/p4 edit ${XcodeAlertAffectedPaths}

