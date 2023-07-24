// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class NVAftermath : ModuleRules
{
    public NVAftermath(ReadOnlyTargetRules Target)
        : base(Target)
	{
		Type = ModuleType.External;

        if (Target.Platform == UnrealTargetPlatform.Win64 && Target.Architecture.bIsX64)
		{
			string ThirdPartyDir = Path.Combine(Target.UEThirdPartySourceDirectory, "NVIDIA", "NVaftermath");
			string IncludeDir = Path.Combine(ThirdPartyDir, "include");
			string LibrariesDir = Path.Combine(ThirdPartyDir, "lib", "x64");
			string BinariesDir = Path.Combine("$(EngineDir)", "Binaries", "ThirdParty", "NVIDIA", "NVaftermath", "Win64");

			PublicDefinitions.Add("NV_AFTERMATH=1");

			PublicSystemIncludePaths.Add(IncludeDir);
            PublicAdditionalLibraries.Add(Path.Combine(LibrariesDir, "GFSDK_Aftermath_Lib.x64.lib"));
			RuntimeDependencies.Add(Path.Combine(BinariesDir, "GFSDK_Aftermath_Lib.x64.dll"));
            PublicDelayLoadDLLs.Add("GFSDK_Aftermath_Lib.x64.dll");
        }
		else
        {
            PublicDefinitions.Add("NV_AFTERMATH=0");
        }
	}
}

