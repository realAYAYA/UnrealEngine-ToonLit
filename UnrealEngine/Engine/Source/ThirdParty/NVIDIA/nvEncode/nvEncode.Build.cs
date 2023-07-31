// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class nvEncode : ModuleRules
{
    public nvEncode(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

        string nvEncodePath = Target.UEThirdPartySourceDirectory + "NVIDIA/nvEncode/";
        PublicSystemIncludePaths.Add(nvEncodePath);
	}
}
