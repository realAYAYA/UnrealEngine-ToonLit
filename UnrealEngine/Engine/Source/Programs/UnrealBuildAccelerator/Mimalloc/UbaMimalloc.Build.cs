// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using Microsoft.Extensions.Logging;

[SupportedPlatforms(UnrealPlatformClass.Desktop)]
[SupportedTargetTypes(TargetType.Program)]
public class UbaMimalloc : ModuleRules
{
	public UbaMimalloc(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;

		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))// || Target.Platform.IsInGroup(UnrealPlatformGroup.Linux))
		{
			PublicDependencyModuleNames.AddRange(new string[] {
				"mimalloc212",
			});
		}

		//PublicDefinitions.AddRange(new string[] {
		//	"UBA_USE_MIMALLOC=1", // compile in mimalloc
		//});

		if (Target.LinkType == TargetLinkType.Modular)
		{
			Logger.LogWarning("UbaMimalloc will not override allocation functions when linked Modular");
		}
	}
}
