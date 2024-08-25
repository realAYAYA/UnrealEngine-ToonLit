// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
public class StormSyncImport : ModuleRules
{
	public StormSyncImport(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"MessageLog",
				"RenderCore",
				"Slate",
				"SlateCore",
				"StormSyncCore",
				"StormSyncTransportCore",
			}
		);

		if (Target.Type == TargetType.Editor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"EditorFramework",
					"SourceControl",
					"StormSyncEditor",
					"UnrealEd",
				}
			);
		}
	}
}