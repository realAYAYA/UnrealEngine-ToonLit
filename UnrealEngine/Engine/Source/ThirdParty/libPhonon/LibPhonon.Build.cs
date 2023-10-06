
// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class libPhonon : ModuleRules
{
    public libPhonon(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;

        string LibraryPath = Target.UEThirdPartySourceDirectory + "libPhonon/phonon_api/";
        string BinaryPath = "$(EngineDir)/Binaries/ThirdParty/Phonon/";

        PublicSystemIncludePaths.Add(LibraryPath + "include");

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            LibraryPath = LibraryPath + "/lib/Win64/";
            PublicAdditionalLibraries.Add(LibraryPath + "phonon.lib");

            string DllName = "phonon.dll";

            // 64 bit only libraries for TAN support:
            string TrueAudioNextDllName = "tanrt64.dll";
            string GPUUtilitiesDllName = "GPUUtilities.dll";

            PublicDelayLoadDLLs.Add(DllName);
            PublicDelayLoadDLLs.Add(TrueAudioNextDllName);
            PublicDelayLoadDLLs.Add(GPUUtilitiesDllName);

            BinaryPath += "Win64/";

            RuntimeDependencies.Add(BinaryPath + DllName);
            RuntimeDependencies.Add(BinaryPath + TrueAudioNextDllName);
            RuntimeDependencies.Add(BinaryPath + GPUUtilitiesDllName);
        }
        else if (Target.Platform == UnrealTargetPlatform.Android)
        {
            PublicAdditionalLibraries.Add(LibraryPath + "/lib/Android/libphonon.so");
        }
    }
}

