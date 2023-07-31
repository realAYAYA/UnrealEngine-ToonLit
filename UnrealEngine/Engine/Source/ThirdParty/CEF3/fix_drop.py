#
# Helper script to reopen any files added in exclusive mode as normal files instead (we don't want locking)
#
import os, io
import subprocess

print("Checking for files opened in exclusive mode...")
result = subprocess.Popen(['p4', 'opened', '...'], stdout=subprocess.PIPE)

for line in io.TextIOWrapper(result.stdout, encoding="utf-8"):
    if "*exclusive*" in line:
        exlFile = line.split("#")[0]
        print(exlFile)
        fileType = "binary"
        if "resource..h" in exlFile:
            fileType = "text"
        subprocess.run(['p4', 'revert', exlFile])
        subprocess.run(['p4', 'add', '-t', fileType, exlFile])

print("Done")