// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class InterchangeEngine : ModuleRules
{
	public InterchangeEngine(ReadOnlyTargetRules Target) : base(Target)
	{
		bTreatAsEngineModule = true;
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"InterchangeCore"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"AssetRegistry",
				"SlateCore",
				"Slate",
				"DeveloperSettings",
				"Json",
				"JsonUtilities",
			}
		);

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"SkeletalMeshUtilitiesCommon",
				}
			);
		}
	}
}
