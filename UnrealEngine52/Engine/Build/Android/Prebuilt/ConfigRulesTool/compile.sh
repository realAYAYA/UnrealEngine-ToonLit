#!/bin/bash
javac -d classes src/*.java
cd classes
jar cfe ../bin/ConfigRulesTool.jar ConfigRulesTool *.class
