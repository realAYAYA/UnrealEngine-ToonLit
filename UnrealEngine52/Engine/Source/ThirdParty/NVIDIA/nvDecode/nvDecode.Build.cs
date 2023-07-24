// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class nvDecode : ModuleRules
{
    public nvDecode(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

        string nvDecodePath = Target.UEThirdPartySourceDirectory + "NVIDIA/nvDecode/";
        PublicSystemIncludePaths.Add(nvDecodePath);
	}
}
