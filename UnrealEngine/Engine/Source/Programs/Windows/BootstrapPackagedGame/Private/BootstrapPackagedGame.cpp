// Copyright Epic Games, Inc. All Rights Reserved.

#include "BootstrapPackagedGame.h"

#define IDI_EXEC_FILE 201
#define IDI_EXEC_ARGS 202

WCHAR* ReadResourceString(HMODULE ModuleHandle, LPCWSTR Name)
{
	WCHAR* Result = NULL;

	HRSRC ResourceHandle = FindResource(ModuleHandle, Name, RT_RCDATA);
	if(ResourceHandle != NULL)
	{
		HGLOBAL AllocHandle = LoadResource(ModuleHandle, ResourceHandle);
		if(AllocHandle != NULL)
		{
			WCHAR* Data = (WCHAR*)LockResource(AllocHandle);
			DWORD DataLen = SizeofResource(ModuleHandle, ResourceHandle) / sizeof(WCHAR);

			Result = new WCHAR[DataLen + 1];
			memcpy(Result, Data, DataLen * sizeof(WCHAR));
			Result[DataLen] = 0;
		}
	}

	return Result;
}

bool TryLoadDll(const WCHAR* ExecDirectory, const WCHAR* Name)
{
	// Try to load it from the system path
	if (LoadLibrary(Name) != nullptr)
	{
		return true;
	}

	// Try to load it from the application directory
	WCHAR AppLocalPath[MAX_PATH];
	PathCombine(AppLocalPath, ExecDirectory, Name);
	if (LoadLibrary(AppLocalPath) != nullptr)
	{
		return true;
	}

	// Otherwise fail
	return false;
}

int InstallMissingPrerequisites(const WCHAR* BaseDirectory, const WCHAR* ExecDirectory)
{
#ifdef _M_X64
	bool bIsX64Target = true;
#else
	bool bIsX64Target = false;
#endif

	// Look for missing prerequisites
	WCHAR MissingPrerequisites[1024] = { 0, };

	// The Microsoft Visual C++ Runtime includes support for VS2015, VS2017, VS2019, and VS2022
	// https://docs.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files?view=msvc-170

	{
		HKEY Hkey;
		LSTATUS KeyOpenStatus;

		if (bIsX64Target)
		{
			KeyOpenStatus = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\VisualStudio\\14.0\\VC\\Runtimes\\x64", 0, KEY_READ, &Hkey);
		}
		else
		{
			// 32bit build running on an 32bit host
			KeyOpenStatus = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\VisualStudio\\14.0\\VC\\Runtimes\\x86", 0, KEY_READ, &Hkey);

			if (ERROR_SUCCESS != KeyOpenStatus)
			{
				// 32bit build running on 64bit host
				KeyOpenStatus = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Wow6432Node\\Microsoft\\VisualStudio\\14.0\\VC\\Runtimes\\x86", 0, KEY_READ, &Hkey);
			}
		}

		bool bInstallVCRedist = true;

		if (KeyOpenStatus == ERROR_SUCCESS)
		{
			auto RegGetDwordOrZero = [](HKEY Hkey, LPCWSTR Name) -> DWORD
			{
				DWORD Value = 0;
				DWORD ValueSize = sizeof Value;
				LSTATUS Status = RegQueryValueExW(Hkey, Name, NULL, NULL, (LPBYTE)&Value, &ValueSize);
				return ERROR_SUCCESS == Status ? Value : 0;
			};

			// This minimum should match the version installed by
			// Engine/Source/Programs/PrereqInstaller/Resources/VCRedist/VC_redist.x64.exe
			const DWORD RequiredMajor = 14;
			const DWORD RequiredMinor = 31;
			const DWORD RequiredBld = 31103;
			const DWORD RequiredRbld = 0;

			const DWORD InstalledMajor = RegGetDwordOrZero(Hkey, L"Major");
			const DWORD InstalledMinor = RegGetDwordOrZero(Hkey, L"Minor");
			const DWORD InstalledBld = RegGetDwordOrZero(Hkey, L"Bld");
			const DWORD InstalledRbld = RegGetDwordOrZero(Hkey, L"Rbld");

			if ((InstalledMajor > RequiredMajor) ||
				(InstalledMajor == RequiredMajor && InstalledMinor > RequiredMinor) ||
				(InstalledMajor == RequiredMajor && InstalledMinor == RequiredMinor && InstalledBld > RequiredBld) ||
				(InstalledMajor == RequiredMajor && InstalledMinor == RequiredMinor && InstalledBld == RequiredBld && InstalledRbld >= RequiredRbld))
			{
				// it is possible that the redist has been uninstalled but the registry entries have not been removed
				// test that some relatively new dlls are able to be loaded
				if (TryLoadDll(ExecDirectory, L"msvcp140_2.dll") &&
					(!bIsX64Target || TryLoadDll(ExecDirectory, L"vcruntime140_1.dll")))
				{
					bInstallVCRedist = false;
				}
			}
			RegCloseKey(Hkey);
		}
		if (bInstallVCRedist)
		{
			wcscat_s(MissingPrerequisites, TEXT("Microsoft Visual C++ Runtime\n"));
		}
	}

	if(!TryLoadDll(ExecDirectory, L"XINPUT1_3.DLL"))
	{
		wcscat_s(MissingPrerequisites, TEXT("DirectX Runtime\n"));
	}

	// Check if there's anything missing
	if(MissingPrerequisites[0] != 0)
	{
		WCHAR MissingPrerequisitesMsg[1024];
		wsprintf(MissingPrerequisitesMsg, L"The following component(s) are required to run this program:\n\n%s", MissingPrerequisites);

		// If we don't have the installer, just notify the user and quit
		WCHAR PrereqInstaller[MAX_PATH];
		if (bIsX64Target)
		{
			PathCombine(PrereqInstaller, BaseDirectory, L"Engine\\Extras\\Redist\\en-us\\UEPrereqSetup_x64.exe");
		}
		else
		{
			PathCombine(PrereqInstaller, BaseDirectory, L"Engine\\Extras\\Redist\\en-us\\UEPrereqSetup_x86.exe");
		}
		if(GetFileAttributes(PrereqInstaller) == INVALID_FILE_ATTRIBUTES)
		{
			MessageBox(NULL, MissingPrerequisitesMsg, NULL, MB_OK);
			return 9001;
		}

		// Otherwise ask them if they want to install them
		wcscat_s(MissingPrerequisitesMsg, L"\nWould you like to install them now?");
		if(MessageBox(NULL, MissingPrerequisitesMsg, NULL, MB_YESNO) == IDNO)
		{
			return 9002;
		}

		// Start the installer
		SHELLEXECUTEINFO ShellExecuteInfo;
		ZeroMemory(&ShellExecuteInfo, sizeof(ShellExecuteInfo));
		ShellExecuteInfo.cbSize = sizeof(ShellExecuteInfo);
		ShellExecuteInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
		ShellExecuteInfo.nShow = SW_SHOWNORMAL;
		ShellExecuteInfo.lpFile = PrereqInstaller;
		if(!ShellExecuteExW(&ShellExecuteInfo))
		{
			return 9003;
		}

		// Wait for the process to complete, then get its exit code
		DWORD ExitCode = 0;
		WaitForSingleObject(ShellExecuteInfo.hProcess, INFINITE);
		GetExitCodeProcess(ShellExecuteInfo.hProcess, &ExitCode);
		CloseHandle(ShellExecuteInfo.hProcess);
		if(ExitCode != 0)
		{
			return 9004;
		}
	}
	return 0;
}

int SpawnTarget(WCHAR* CmdLine)
{
	STARTUPINFO StartupInfo;
	ZeroMemory(&StartupInfo, sizeof(StartupInfo));
	StartupInfo.cb = sizeof(StartupInfo);

	PROCESS_INFORMATION ProcessInfo;
	ZeroMemory(&ProcessInfo, sizeof(ProcessInfo));

	if(!CreateProcess(NULL, CmdLine, NULL, NULL, TRUE, 0, NULL, NULL, &StartupInfo, &ProcessInfo))
	{
		DWORD ErrorCode = GetLastError();

		WCHAR* Buffer = new WCHAR[wcslen(CmdLine) + 50];
		wsprintf(Buffer, L"Couldn't start:\n%s\nCreateProcess() returned %x.", CmdLine, ErrorCode);
		MessageBoxW(NULL, Buffer, NULL, MB_OK);
		delete[] Buffer;

		return 9005;
	}

	WaitForSingleObject(ProcessInfo.hProcess, INFINITE);
	DWORD ExitCode = 9006;
	GetExitCodeProcess(ProcessInfo.hProcess, &ExitCode);

	CloseHandle(ProcessInfo.hThread);
	CloseHandle(ProcessInfo.hProcess);
	return (int)ExitCode;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, TCHAR* CmdLine, int ShowCmd)
{
	(void)hPrevInstance;
	(void)ShowCmd;

	// Get the current module filename
	WCHAR CurrentModuleFile[MAX_PATH];
	GetModuleFileNameW(hInstance, CurrentModuleFile, sizeof(CurrentModuleFile));

	// Get the base directory from the current module filename
	WCHAR BaseDirectory[MAX_PATH];
	PathCanonicalize(BaseDirectory, CurrentModuleFile);
	PathRemoveFileSpec(BaseDirectory);

	// Get the executable to run
	WCHAR* ExecFile = ReadResourceString(hInstance, MAKEINTRESOURCE(IDI_EXEC_FILE));
	if(ExecFile == NULL)
	{
		MessageBoxW(NULL, L"This program is used for packaged games and is not meant to be run directly.", NULL, MB_OK);
		return 9000;
	}

	// Get the directory containing the target to be executed
	WCHAR* TempExecDirectory = new WCHAR[wcslen(BaseDirectory) + wcslen(ExecFile) + 20];
	wsprintf(TempExecDirectory, L"%s\\%s", BaseDirectory, ExecFile);
	WCHAR ExecDirectory[MAX_PATH];
	PathCanonicalize(ExecDirectory, TempExecDirectory);
	delete[] TempExecDirectory;
	PathRemoveFileSpec(ExecDirectory);

	// Create a full command line for the program to run
	WCHAR* BaseArgs = ReadResourceString(hInstance, MAKEINTRESOURCE(IDI_EXEC_ARGS));
	size_t ChildCmdLineLength = wcslen(BaseDirectory) + wcslen(ExecFile) + wcslen(BaseArgs) + wcslen(CmdLine) + 20;
	WCHAR* ChildCmdLine = new WCHAR[ChildCmdLineLength];
	swprintf(ChildCmdLine, ChildCmdLineLength, L"\"%s\\%s\" %s %s", BaseDirectory, ExecFile, BaseArgs, CmdLine);
	delete[] BaseArgs;
	delete[] ExecFile;

	// Install the prerequisites
	int ExitCode = InstallMissingPrerequisites(BaseDirectory, ExecDirectory);
	if(ExitCode != 0)
	{
		delete[] ChildCmdLine;
		return ExitCode;
	}

	// Spawn the target executable
	ExitCode = SpawnTarget(ChildCmdLine);
	if(ExitCode != 0)
	{
		delete[] ChildCmdLine;
		return ExitCode;
	}

	delete[] ChildCmdLine;
	return ExitCode;
}
