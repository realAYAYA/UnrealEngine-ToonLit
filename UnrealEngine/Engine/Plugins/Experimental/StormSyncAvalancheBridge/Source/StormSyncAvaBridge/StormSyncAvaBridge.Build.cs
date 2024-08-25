// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class StormSyncAvaBridge : ModuleRules
{
	public StormSyncAvaBridge(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AvalancheMedia",
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"StormSyncCore",
				"StormSyncTransportCore",
				"StormSyncTransportServer",
				"StormSyncTransportClient",
			}
		);
	}
}
