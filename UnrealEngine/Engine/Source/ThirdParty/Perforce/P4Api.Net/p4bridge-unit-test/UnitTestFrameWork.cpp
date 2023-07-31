#include "StdAfx.h"
#include "UnitTestFrameWork.h"
#include <excpt.h>
#include <Shellapi.h>

#include <vector>

using std::vector;

void UnitTestSuite::RegisterTest(UnitTest * test, char * testName)
{
	TestList * newTest = new TestList();
	newTest->TestName = testName;
	newTest->Test = test;
	newTest->pNext = 0;

	if (pLastTest)
	{
		pLastTest->pNext = newTest;
		pLastTest = newTest;
	}
	else
	{
		pFirstTest = newTest;
		pLastTest = newTest;
	}
}

UnitTestSuite::UnitTestSuite()
{
	pFirstTest = 0;
	pLastTest = 0;

	pNextTestSuite = 0;
}

UnitTestSuite::~UnitTestSuite()
{
	TestList * pCurTest = pFirstTest;
	while(pCurTest)
	{
		pFirstTest = pFirstTest->pNext;
		delete pCurTest;
		pCurTest = pFirstTest;
	}

	pFirstTest = 0;
	pLastTest = 0;

	pNextTestSuite = 0;
}

#define HANDLE_EXCEPTION() HandleException(__FILE__, __LINE__, __FUNCTION__, GetExceptionCode(), GetExceptionInformation())

int UnitTestSuite::HandleException(const char* fname, unsigned int line, const char* func, unsigned int c, struct _EXCEPTION_POINTERS *e)
{
	unsigned int code = 0;
	struct _EXCEPTION_POINTERS *ep = NULL;
	
	code = c;
	ep = e;

	printf("\t%s(%d): %s : Exception Detected: ", fname, line, func);

	switch (code)
	{
	case EXCEPTION_ACCESS_VIOLATION:
		printf("EXCEPTION_ACCESS_VIOLATION\r\n");
		break;
	case EXCEPTION_DATATYPE_MISALIGNMENT:
		printf("EXCEPTION_DATATYPE_MISALIGNMENT\r\n");
		break;
	case EXCEPTION_BREAKPOINT:
		printf("EXCEPTION_BREAKPOINT\r\n");
		break;
	case EXCEPTION_SINGLE_STEP:
		printf("EXCEPTION_SINGLE_STEP\r\n");
		break;
	case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
		printf("EXCEPTION_ARRAY_BOUNDS_EXCEEDED\r\n");
		break;
	case EXCEPTION_FLT_DENORMAL_OPERAND:
		printf("EXCEPTION_FLT_DENORMAL_OPERAND\r\n");
		break;
	case EXCEPTION_FLT_DIVIDE_BY_ZERO:
		printf("EXCEPTION_FLT_DIVIDE_BY_ZERO\r\n");
		break;
	case EXCEPTION_FLT_INEXACT_RESULT:
		printf("EXCEPTION_FLT_INEXACT_RESULT\r\n");
		break;
	case EXCEPTION_FLT_INVALID_OPERATION:
		printf("EXCEPTION_FLT_INVALID_OPERATION\r\n");
		break;
	case EXCEPTION_FLT_OVERFLOW:
		printf("EXCEPTION_FLT_OVERFLOW\r\n");
		break;
	case EXCEPTION_FLT_STACK_CHECK:
		printf("EXCEPTION_FLT_STACK_CHECK\r\n");
		break;
	case EXCEPTION_FLT_UNDERFLOW:
		printf("EXCEPTION_FLT_UNDERFLOW\r\n");
		break;
	case EXCEPTION_INT_DIVIDE_BY_ZERO:
		printf("EXCEPTION_INT_DIVIDE_BY_ZERO\r\n");
		break;
	case EXCEPTION_INT_OVERFLOW:
		printf("EXCEPTION_INT_OVERFLOW\r\n");
		break;
	case EXCEPTION_PRIV_INSTRUCTION:
		printf("EXCEPTION_PRIV_INSTRUCTION\r\n");
		break;
	case EXCEPTION_IN_PAGE_ERROR:
		printf("EXCEPTION_IN_PAGE_ERROR\r\n");
		break;
	case EXCEPTION_ILLEGAL_INSTRUCTION:
		printf("EXCEPTION_ILLEGAL_INSTRUCTION\r\n");
		break;
	case EXCEPTION_NONCONTINUABLE_EXCEPTION:
		printf("EXCEPTION_NONCONTINUABLE_EXCEPTION\r\n");
		break;
	case EXCEPTION_STACK_OVERFLOW:
		printf("EXCEPTION_STACK_OVERFLOW\r\n");
		break;
	case EXCEPTION_INVALID_DISPOSITION:
		printf("EXCEPTION_INVALID_DISPOSITION\r\n");
		break;
	case EXCEPTION_GUARD_PAGE:
		printf("EXCEPTION_GUARD_PAGE\r\n");
		break;
	case EXCEPTION_INVALID_HANDLE:
		printf("EXCEPTION_INVALID_HANDLE\r\n");
		break;
	default:
		printf("UNKNOWN EXCEPTION\r\n");
		break;
	}
	return EXCEPTION_EXECUTE_HANDLER;
}

LPPROCESS_INFORMATION UnitTestSuite::RunProgram(char * cmdLine, char * cwd, bool newConsole, bool waitForExit)
{
	LPSTARTUPINFOA si = new STARTUPINFOA;
	LPPROCESS_INFORMATION pi = new PROCESS_INFORMATION;

	ZeroMemory( si, sizeof(STARTUPINFOA) );
	si->cb = sizeof(si);
	ZeroMemory( pi, sizeof(PROCESS_INFORMATION) );

	// Start the child process. 
	if( !CreateProcessA( 
		NULL,           // No module name (use command line)
		cmdLine,        // Command line
		NULL,           // Process handle not inheritable
		NULL,           // Thread handle not inheritable
		FALSE,          // Set handle inheritance to FALSE
		newConsole?CREATE_NEW_CONSOLE:0,              // No creation flags
		NULL,           // Use parent's environment block
		cwd,            // Starting directory 
		si,             // Pointer to STARTUPINFO structure
		pi )            // Pointer to PROCESS_INFORMATION structure
	) 
	{
		printf( "CreateProcess failed (%d).\n", GetLastError() );
		return NULL;
	}
	if (waitForExit)
	{
		// Wait until child process exits.
		WaitForSingleObject( pi->hProcess, INFINITE );

		// Close process and thread handles. 
		CloseHandle( pi->hProcess );
		CloseHandle( pi->hThread );
	}
	delete si;

	return pi;
}

bool UnitTestSuite::EndProcess(LPPROCESS_INFORMATION pi)
{
	TerminateProcess( pi->hProcess, 0);
	
	// Wait until child process exits.
	WaitForSingleObject( pi->hProcess, INFINITE );

	// Close process and thread handles. 
	CloseHandle( pi->hProcess );
	CloseHandle( pi->hThread );

	delete pi;

	return true;
}

bool UnitTestSuite::rmDir(char * path)
{
	char szDir[MAX_PATH+1];  // +1 for the double null terminate

	SHFILEOPSTRUCTA fos = {0};

	strcpy(szDir, path);
	int len = static_cast<int>(strlen(szDir));
	szDir[len+1] = 0; // double null terminate for SHFileOperation

	// delete the folder and everything inside
	fos.wFunc = FO_DELETE;
	fos.pFrom = szDir;
	fos.fFlags = FOF_NO_UI;
	return (SHFileOperation(&fos) != 0);
}

bool UnitTestFrameWork::isSkipTest(const char* pTestName) {
	if (matchName.empty())
		return false;
	// TODO: more complex regex or something
	return (matchName != pTestName);
}

void UnitTestSuite::RunTests()
{
	TestList * pCurrentTest = pFirstTest;

	while (pCurrentTest)
	{
		// if we are we skipping tests, do it now
		if (UnitTestFrameWork::isSkipTest(pCurrentTest->TestName)) {
			pCurrentTest = pCurrentTest->pNext;
			continue;
		}

		unsigned int code = 0;
		struct _EXCEPTION_POINTERS *ep = NULL;

		int itemCounts[p4typesCount];
		__try
		{
			printf("Test %s:\r\n", pCurrentTest->TestName);

			// record the current ItemCounts to make sure that we're freeing everything
			for (int i = 0; i < p4typesCount; i++)
				itemCounts[i] = p4base::GetItemCount(i);
			
			int iAllocs = Utils::AllocCount(), iFrees = Utils::FreeCount();

			if (!Setup())
			{
				printf("\tSetup  Failed!!\r\n");
				UnitTestFrameWork::IncrementTestsFailed();

				if (endOnFailure)
					return;
			}
			else
			{
				bool passed = (*pCurrentTest->Test)();
				bool tearDownPassed = TearDown(pCurrentTest->TestName);
				// check that the ItemCounts match
				int itemMismatch = 0;
				for (int i = 0; i < p4typesCount; i++)
				{
					if (p4base::GetItemCount(i) != itemCounts[i])
					{
						printf("\t<<<<*** Item count for %s mismatch: %d/%d\n", p4base::GetTypeStr(i), itemCounts[i], p4base::GetItemCount());
						itemMismatch += p4base::GetItemCount(i) - itemCounts[i];
					}
				}

				// check the string alloc counts
				int stringAllocs = (Utils::AllocCount() - iAllocs), stringFrees = (Utils::FreeCount() - iFrees);
				if (stringAllocs != stringFrees)
					printf("\t<<<<*** String alloc count mismatch: %d/%d\n", stringAllocs, stringFrees);

				passed = passed && (itemMismatch == 0) && (stringAllocs == stringFrees);
				if (passed)
					printf("\tPassed\r\n");
				else 
					printf("\t<<<<***Failed!!***>>>>\r\n");

				if (!tearDownPassed)
					printf("\tTearDown Failed!!\r\n");

				if (!passed || !tearDownPassed)
				{
					UnitTestFrameWork::IncrementTestsFailed();
					if (endOnFailure)
						return;
				}
				UnitTestFrameWork::IncrementTestsPassed();
			}
		} 
		__except (HANDLE_EXCEPTION())
		{
			UnitTestFrameWork::IncrementTestsFailed();
			if (endOnFailure)
				return;
		}
		pCurrentTest = pCurrentTest->pNext;
	}

	if (pNextTestSuite)
		pNextTestSuite->RunTests();
}

bool UnitTestSuite::Assert(bool condition, char* FailStr, int Line, char * FileName)
{
	if (condition) // It's as expected
		return true;

	if (breakOnFailure)
		__debugbreak();

	printf("\t%s: Line: %d, %s:\r\n", FailStr, Line, FileName);
	return false;
}

bool UnitTestSuite::breakOnFailure = false;

bool UnitTestSuite::endOnFailure = false;

UnitTestFrameWork::UnitTestFrameWork(void)
{
}

UnitTestSuite * UnitTestFrameWork::pFirstTestSuite = 0;
UnitTestSuite * UnitTestFrameWork::pLastTestSuite = 0;

void UnitTestFrameWork::RegisterTestSuite(UnitTestSuite * pSuite)
{
	if (pLastTestSuite)
	{
		pLastTestSuite->NextTestSuite(pSuite);
		pLastTestSuite = pSuite;
	}
	else
	{
		pFirstTestSuite = pSuite;
		pLastTestSuite = pSuite;
	}
}

int UnitTestFrameWork::testsPassed = 0;
int UnitTestFrameWork::testsFailed = 0;
std::string UnitTestFrameWork::matchName;

void UnitTestFrameWork::RunTests()
{
	testsPassed = 0;
	testsFailed = 0;

	if (pFirstTestSuite)
		pFirstTestSuite->RunTests();

	printf("Tests Passed %d, TestFailed: %d\r\n", testsPassed, testsFailed);

	// delete all the test suites
	while (pFirstTestSuite)
	{
		UnitTestSuite * pCurrentTestSuite = pFirstTestSuite;
		pFirstTestSuite = pCurrentTestSuite->NextTestSuite();
		delete pCurrentTestSuite;
	}
	p4base::Cleanup();
}
