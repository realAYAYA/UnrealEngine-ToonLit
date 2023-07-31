#!/bin/bash
javac -d classes src/*.java
cd classes
jar cfe ../bin/GenUniversalAPK.jar GenUniversalAPK *.class
