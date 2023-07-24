#!/bin/bash
set -e

# First start p4d and wait for it to finish starting
p4d &
sleep 3

# Continue running perforce server -- tail the log
tail -f $P4LOGS