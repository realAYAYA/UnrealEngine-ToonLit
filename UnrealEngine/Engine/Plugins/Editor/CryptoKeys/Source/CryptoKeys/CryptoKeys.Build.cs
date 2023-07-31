// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CryptoKeys : ModuleRules
{
	public CryptoKeys(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.Add("CryptoKeys/Classes");

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"ApplicationCore",
				"Engine",
				"EditorFramework",
				"UnrealEd",
				"CryptoKeysOpenSSL",
				"Slate",
				"SlateCore",
				"GameProjectGeneration",
				"DeveloperToolSettings"
		});
	}
}