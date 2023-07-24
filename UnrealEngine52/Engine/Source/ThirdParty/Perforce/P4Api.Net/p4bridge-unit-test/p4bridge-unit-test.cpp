// p4bridge-unit-test.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "UnitTestFrameWork.h"

#include <conio.h>
#include <string.h>

int main(int argc, char* argv[])
{
    if (argc > 0)
        for (int idx = 1; idx < argc; idx++)
        {
            if (strcmp(argv[idx], "-b") == 0) // break on fail
                UnitTestSuite::BreakOnFailure(true); 
            if (strcmp(argv[idx], "-e") == 0) // end on fail
                UnitTestSuite::EndOnFailure(true); 
			// assume this is a test name to match (don't run tests that do not match)
			UnitTestFrameWork::AddTestMatch(argv[idx]);
        }
    UnitTestFrameWork::RunTests();

	p4base::PrintMemoryState("After test complete");
	p4base::DumpMemoryState("After test complete");

    printf("Hit 'x' to exit");
    _getch();

    return 0;
}

