// Copyright Epic Games, Inc. All Rights Reserved.

#include "BootstrapPackagedGame.h"

#define IDI_EXEC_FILE 201
#define IDI_EXEC_ARGS 202

struct VersionInfo
{
	DWORD Major = 0;
	DWORD Minor = 0;
	DWORD Bld = 0;
	DWORD Rbld = 0;
};

// This minimum should match the version installed by
// Engine/Source/Programs/PrereqInstaller/Resources/VCRedist/VC_redist.x64.exe
static const VersionInfo MinRedistVersion = { 14, 38, 33130, 0 };

bool IsVersionValid(const VersionInfo& Version, const VersionInfo& MinVersion)
{
	if (Version.Major > MinVersion.Major) return true;
	if (Version.Major == MinVersion.Major && Version.Minor > MinVersion.Minor) return true;
	if (Version.Major == MinVersion.Major && Version.Minor == MinVersion.Minor && Version.Bld > MinVersion.Bld) return true;
	if (Version.Major == MinVersion.Major && Version.Minor == MinVersion.Minor && Version.Bld == MinVersion.Bld && Version.Rbld >= MinVersion.Rbld) return true;
	return false;
}

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
	WCHAR AppLocalPath[MAX_PATH];
	if (PathCombine(AppLocalPath, ExecDirectory, Name) == nullptr)
	{
		return false;
	}
	HMODULE Handle = LoadLibrary(AppLocalPath);
	if (Handle != nullptr)
	{
		FreeLibrary(Handle);
		return true;
	}
	return false;
}

bool TryLoadDll(const WCHAR* Name)
{
	return TryLoadDll(nullptr, Name);
}

bool TryGetFileVersionInfo(const WCHAR* ExecDirectory, const WCHAR* Name, VersionInfo& outVersionInfo)
{
	WCHAR Path[MAX_PATH];
	if (PathCombine(Path, ExecDirectory, Name) == nullptr)
	{
		return false;
	}

	DWORD VersionSize = GetFileVersionInfoSize(Path, nullptr);
	if (VersionSize == 0)
	{
		return false;
	}

	LPSTR pVersionData = new char[VersionSize];
	if (!GetFileVersionInfo(Path, 0, VersionSize, pVersionData))
	{
		delete[] pVersionData;
		return false;
	}

	VS_FIXEDFILEINFO* pFileInfo = nullptr;
	UINT FileInfoLen = 0;
	if (!VerQueryValue(pVersionData, L"\\", (LPVOID*)&pFileInfo, &FileInfoLen) && FileInfoLen != 0)
	{
		delete[] pVersionData;
		return false;
	}

	outVersionInfo.Major = (pFileInfo->dwFileVersionMS >> 16) & 0xffff;
	outVersionInfo.Minor = (pFileInfo->dwFileVersionMS >> 0) & 0xffff;
	outVersionInfo.Bld = (pFileInfo->dwFileVersionLS >> 16) & 0xffff;
	outVersionInfo.Rbld = (pFileInfo->dwFileVersionLS >> 0) & 0xffff;

	delete[] pVersionData;
	return true;
}

bool TryGetFileVersionInfo(const WCHAR* Name, VersionInfo& outVersionInfo)
{
	return TryGetFileVersionInfo(nullptr, Name, outVersionInfo);
}

bool IsDllValid(const WCHAR* ExecDirectory, const WCHAR* Name, const VersionInfo& RequiredVersion)
{
	VersionInfo DllInfo;
	return TryGetFileVersionInfo(ExecDirectory, Name, DllInfo) && IsVersionValid(DllInfo, RequiredVersion) && TryLoadDll(ExecDirectory, Name);
}

bool IsDllValid(const WCHAR* Name, const VersionInfo& RequiredVersion)
{
	return IsDllValid(nullptr, Name, RequiredVersion);
}

int InstallMissingPrerequisites(const WCHAR* BaseDirectory, const WCHAR* ExecDirectory)
{
	// Look for missing prerequisites
	WCHAR MissingPrerequisites[1024] = { 0, };

	// The Microsoft Visual C++ Runtime includes support for VS2015, VS2017, VS2019, and VS2022
	// https://docs.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files?view=msvc-170
	
	{
		bool bInstallVCRedist = true;

		// Check the file version of bundled redist dlls
		if (IsDllValid(ExecDirectory, L"msvcp140_2.dll", MinRedistVersion) &&
			IsDllValid(ExecDirectory, L"vcruntime140_1.dll", MinRedistVersion))
		{
			bInstallVCRedist = false;
		}

		// If no bundled redist dlls are available, check the registry for the installed redist, 
		if (bInstallVCRedist)
		{
			HKEY Hkey;
			LSTATUS KeyOpenStatus = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\VisualStudio\\14.0\\VC\\Runtimes\\x64", 0, KEY_READ, &Hkey);

			if (KeyOpenStatus == ERROR_SUCCESS)
			{
				auto RegGetDwordOrZero = [](HKEY Hkey, LPCWSTR Name) -> DWORD
					{
						DWORD Value = 0;
						DWORD ValueSize = sizeof Value;
						LSTATUS Status = RegQueryValueExW(Hkey, Name, NULL, NULL, (LPBYTE)&Value, &ValueSize);
						return ERROR_SUCCESS == Status ? Value : 0;
					};

				const VersionInfo InstalledVersion = {
					RegGetDwordOrZero(Hkey, L"Major"),
					RegGetDwordOrZero(Hkey, L"Minor"),
					RegGetDwordOrZero(Hkey, L"Bld"),
					RegGetDwordOrZero(Hkey, L"Rbld")
				};

				RegCloseKey(Hkey);

				if (IsVersionValid(InstalledVersion, MinRedistVersion))
				{
					// it is possible that the redist has been uninstalled but the registry entries have not been removed
					// test that some relatively new dlls are able to be loaded
					if (IsDllValid(L"msvcp140_2.dll", MinRedistVersion) &&
						IsDllValid(L"vcruntime140_1.dll", MinRedistVersion))
					{
						bInstallVCRedist = false;
					}
				}
			}
			if (bInstallVCRedist)
			{
				wcscat_s(MissingPrerequisites, TEXT("Microsoft Visual C++ Runtime\n"));
			}
		}
	}


	// Check if there's anything missing
	if(MissingPrerequisites[0] != 0)
	{
		WCHAR MissingPrerequisitesMsg[1024];
		wsprintf(MissingPrerequisitesMsg, L"The following component(s) are required to run this program:\n\n%s", MissingPrerequisites);

		// If we don't have the installer, just notify the user and quit
		WCHAR PrereqInstaller[MAX_PATH];
		PathCombine(PrereqInstaller, BaseDirectory, L"Engine\\Extras\\Redist\\en-us\\UEPrereqSetup_x64.exe");
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

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR CmdLine, _In_ int ShowCmd)
{
	(void)hPrevInstance;
	(void)ShowCmd;

	// Get the current module filename
	WCHAR CurrentModuleFile[MAX_PATH];
	GetModuleFileNameW(hInstance, CurrentModuleFile, MAX_PATH);

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
