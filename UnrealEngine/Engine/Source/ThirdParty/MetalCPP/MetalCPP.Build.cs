// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class MetalCPP : ModuleRules
{
	public MetalCPP(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string MTLCPPPath = Target.UEThirdPartySourceDirectory + "MetalCPP/";

		if (Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.IOS ||
            Target.Platform == UnrealTargetPlatform.TVOS || Target.Platform == UnrealTargetPlatform.VisionOS)
		{
			PublicSystemIncludePaths.Add(MTLCPPPath);
		}
    }
}
