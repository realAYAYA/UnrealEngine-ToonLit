// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;
using System;
using EpicGames.Core;


public class MsQuic : ModuleRules
{
    private string GetModulePath()
    {
        return Path.GetFullPath(ModuleDirectory);
    }

    public MsQuic(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;

        bool bShouldUseMsQuic = false;

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            bShouldUseMsQuic = true;
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            bShouldUseMsQuic = false;
        }
        else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix) && Target.Architecture == UnrealArch.X64)
        {
            bShouldUseMsQuic = true;
        }

        string EngineDir = Path.GetFullPath(Target.RelativeEnginePath);
        string BinariesDir = Path.Combine(EngineDir, "Binaries", "ThirdParty");

		if (bShouldUseMsQuic)
        {
            // Current version MsQuic v2.2.0 released on 18 Apr 2023
            string MsQuicSdkPath = Path.Combine(GetModulePath(), "v220");
            string MsQuicBinariesPath = Path.Combine(BinariesDir, "MsQuic", "v220");

            if (Target.Platform == UnrealTargetPlatform.Win64)
            {
                PublicDefinitions.Add("MSQUIC_WIN=1");

                string IncludePath = Path.Combine(MsQuicSdkPath, "win64", "include");
                PublicSystemIncludePaths.Add(IncludePath);

                string LibPath = Path.Combine(MsQuicSdkPath, "win64", "lib");
                string MsQuicLib = Path.Combine(LibPath, "msquic.lib");

                string DllPath = Path.Combine(MsQuicBinariesPath, "win64");
                string MsQuicDll = Path.Combine(DllPath, "msquic.dll");
				
				PublicAdditionalLibraries.Add(MsQuicLib);
				PublicDelayLoadDLLs.Add("msquic.dll");
				RuntimeDependencies.Add(MsQuicDll);
			}
			else if (Target.Platform == UnrealTargetPlatform.Linux)
            {
                PublicDefinitions.Add("MSQUIC_LINUX=1");
                PublicDefinitions.Add("MSQUIC_POSIX=1");

                string IncludePath = Path.Combine(MsQuicSdkPath, "linux", "include");
                PublicSystemIncludePaths.Add(IncludePath);

                string LibraryPath = Path.Combine(MsQuicBinariesPath, "linux");
                string MsQuicLibrary = Path.Combine(LibraryPath, "libmsquic.so");

				PublicAdditionalLibraries.Add(MsQuicLibrary);
                PublicDelayLoadDLLs.Add(MsQuicLibrary);
				RuntimeDependencies.Add(MsQuicLibrary);
				RuntimeDependencies.Add(Path.Combine(LibraryPath, "libmsquic.so.2"));
			}
            else if (Target.Platform == UnrealTargetPlatform.Mac)
            {
                PublicDefinitions.Add("MSQUIC_POSIX=1");

                string IncludePath = Path.Combine(MsQuicSdkPath, "macos", "include");
                PublicSystemIncludePaths.Add(IncludePath);

                string LibraryPath = Path.Combine(MsQuicBinariesPath, "macos");
                string MsQuicLibrary = Path.Combine(LibraryPath, "libmsquic.dylib");

                PublicAdditionalLibraries.Add(BinariesDir);
                PublicDelayLoadDLLs.Add(MsQuicLibrary);
				RuntimeDependencies.Add(MsQuicLibrary);
            }
        }
    }
}
