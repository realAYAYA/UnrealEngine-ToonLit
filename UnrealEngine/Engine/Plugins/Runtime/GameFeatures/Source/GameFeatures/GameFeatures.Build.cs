// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class GameFeatures : ModuleRules
	{
        public GameFeatures(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
                {
                    "Core",
                    "CoreUObject",
					"DeveloperSettings",
					"Engine",
                    "ModularGameplay",
					"DataRegistry"
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"AssetRegistry",
					"GameplayTags",
					"InstallBundleManager",
					"Json",
					"JsonUtilities",
					"PakFile",
					"Projects",
					"RenderCore", // required for FDeferredCleanupInterface
					"TraceLog",
				}
			);

			if (Target.bBuildEditor)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
						"UnrealEd",
						"PluginUtils",
					}
				);
			}
		}
	}
}
