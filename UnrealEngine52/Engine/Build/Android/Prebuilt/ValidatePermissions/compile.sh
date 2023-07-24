#!/bin/bash
javac -d classes src/*.java
cd classes
jar cfe ../bin/ValidatePermissions.jar ValidatePermissions *.class
