// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class jemalloc : ModuleRules
{
	public jemalloc(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
        {
		    // includes may differ depending on target platform
		    PublicIncludePaths.Add(Target.UEThirdPartySourceDirectory + "jemalloc/include/Unix/" + Target.Architecture);
			PublicAdditionalLibraries.Add(Target.UEThirdPartySourceDirectory + "jemalloc/lib/Unix/" + Target.Architecture + "/libjemalloc_pic.a");
        }
	}
}
