// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;
using System;
public class NVAftermath : ModuleRules
{
    public NVAftermath(ReadOnlyTargetRules Target)
        : base(Target)
	{
		Type = ModuleType.External;


        if (Target.Platform == UnrealTargetPlatform.Win64)
		{
            String NVAftermathPath = Target.UEThirdPartySourceDirectory + "NVIDIA/NVaftermath/";
            PublicSystemIncludePaths.Add(NVAftermathPath + "include");
            
            String NVAftermathLibPath = NVAftermathPath + "lib/x64/";
            PublicAdditionalLibraries.Add(NVAftermathLibPath + "GFSDK_Aftermath_Lib.x64.lib");

            String AftermathDllName = "GFSDK_Aftermath_Lib.x64.dll";                  
            String nvDLLPath = "$(EngineDir)/Binaries/ThirdParty/NVIDIA/NVaftermath/Win64/" + AftermathDllName;
            PublicDelayLoadDLLs.Add(AftermathDllName);
            RuntimeDependencies.Add(nvDLLPath);

            PublicDefinitions.Add("NV_AFTERMATH=1");
        }
		else
        {
            PublicDefinitions.Add("NV_AFTERMATH=0");
        }
	}
}

