// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class VCamCore : ModuleRules
{
	public VCamCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);


		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);


		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Composure",
				"Core",
				"VPUtilities",
				"EnhancedInput",
				"VCamInput"
				// ... add other public dependencies that you statically link with here ...
			}
			);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"UMG",
				"CinematicCamera",
				"GameplayTags",
				"LiveLinkInterface",
				"MediaIOCore",
				"RemoteSession",
				"InputCore",
				"VPRoles"
				// ... add private dependencies that you statically link with here ...
			}
			);


		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);

		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"EditorFramework",
					"LevelEditor",
					"UnrealEd",
					"Concert",
					"ConcertSyncClient",
					"MultiUserClient",
					"InputEditor"
				}
			);
		}
	}
}
