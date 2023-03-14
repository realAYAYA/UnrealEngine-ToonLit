// Copyright Epic Games, Inc. All Rights Reserved.

#if PLATFORM_WINDOWS
	#include <windows.h>
#else
	#include <unistd.h>
	#include <stdlib.h>
#endif

#include "ShellOpenDocument.h"
#include <errno.h>
#include <stdexcept>

// Make the shell to open the document (URL or Path)
void UE_AC::ShellOpenDocument(const char* InDocRef)
{
#ifdef __GNUC__
	int ForkResult = fork();
	if (ForkResult == 0)
	{
		// The new process execute the converter
		int resultexecl = execl("/usr/bin/open", "/usr/bin/open", InDocRef, (char*)NULL);
		fprintf(stderr, "ShellOpenDocument - execl result = %d, errno = %d\n", resultexecl, errno);
		exit(EXIT_FAILURE);
	}
	else if (ForkResult != -1)
	{
		int status;

		// wait for child process.
		waitpid(ForkResult, &status, 0);
	#ifdef DEBUG
		printf("ShellOpenDocument returned %d for document %s", status, InDocRef);
	#endif
	}
#else
	::ShellExecuteA(NULL, "open", InDocRef, NULL, NULL, SW_SHOW);
#endif
}

// Start the product manager
void UE_AC::ShellStartProductManager()
{
#ifdef __GNUC__
	int ForkResult = fork();
	if (ForkResult == 0)
	{
		// The new process execute the converter
		int resultexecl = execl("/usr/bin/open", "/usr/bin/open", "-b", "com.abvent.productmanager", (char*)NULL);
		fprintf(stderr, "ShellStartProductManager - execl result = %d, errno = %d\n", resultexecl, errno);
		exit(EXIT_FAILURE);
	}
	else if (ForkResult != -1)
	{
		int status;

		// wait for child process.
		waitpid(ForkResult, &status, 0);
	#ifdef DEBUG
		printf("ShellStartProductManager returned %d\n", status);
	#endif
	}
#else
	wchar_t keyValue[MAX_PATH];
	DWORD	DataSize = MAX_PATH;
	LONG	keyResult = ::RegGetValueW(HKEY_LOCAL_MACHINE,
									   L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\Product Manager.exe",
									   nullptr, RRF_RT_ANY, nullptr, &keyValue, &DataSize);
	if (keyResult == ERROR_SUCCESS)
	{
		::STARTUPINFOW si;
		ZeroMemory(&si, sizeof(si));
		::PROCESS_INFORMATION pi;
		ZeroMemory(&pi, sizeof(pi));
		if (::CreateProcessW(keyValue, nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
		{
			::CloseHandle(pi.hProcess);
			::CloseHandle(pi.hThread);
		}
		else
		{
			printf("ShellStartProdutManager - Error when creating process %d\n", ::GetLastError());
		}
	}
	else
	{
		printf("ShellStartProductManager RegGetValueW returned %d\n", keyResult);
	}
#endif
}
