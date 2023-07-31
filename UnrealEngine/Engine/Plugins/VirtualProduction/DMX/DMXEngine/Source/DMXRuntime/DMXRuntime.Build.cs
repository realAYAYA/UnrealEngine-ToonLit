// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DMXRuntime : ModuleRules
{
	public DMXRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
                "CoreUObject",
				"DatasmithContent",
				"Engine",
				"DMXProtocol",
				"JsonUtilities",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AssetRegistry",
				"DMXFixtureActorInterface",
				"Json",
				"MovieScene",
				"Projects",
				"XmlParser",
			}
		);

		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"UnrealEd",
				}
			);
		}
	}
}
