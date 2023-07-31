// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AssetRegistryExport : ModuleRules
{
	public AssetRegistryExport(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Engine"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"SQLiteCore",
				"AssetRegistry",
			}
		);
	}
}
