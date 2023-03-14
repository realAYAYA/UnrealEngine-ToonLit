// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class Amf : ModuleRules
{
    public Amf(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

        string AmfPath = Target.UEThirdPartySourceDirectory + "AMD/Amf/";
        PublicSystemIncludePaths.Add(AmfPath);
	}
}
