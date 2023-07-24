// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;
using System;

public class GPUDirect : ModuleRules
{
	public GPUDirect(ReadOnlyTargetRules Target)
		: base(Target)
	{
		Type = ModuleType.External;

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			String DVPPath = Target.UEThirdPartySourceDirectory + "NVIDIA/GPUDirect/";
			PublicSystemIncludePaths.Add(DVPPath + "include");

			String DVPLibPath = DVPPath + "lib/Windows/x64/";
			PublicAdditionalLibraries.Add(DVPLibPath + "dvp.lib");


			String DVPDLLFullName = "dvp.dll";
			String DVPDLLPath = "$(EngineDir)/Binaries/ThirdParty/NVIDIA/GPUDirect/Win64/" + DVPDLLFullName;

			PrivateRuntimeLibraryPaths.Add("$(EngineDir)/Binaries/ThirdParty/NVIDIA/GPUDirect/Win64");
			PublicDelayLoadDLLs.Add(DVPDLLFullName);
			RuntimeDependencies.Add(DVPDLLPath);
		}
	}
}

