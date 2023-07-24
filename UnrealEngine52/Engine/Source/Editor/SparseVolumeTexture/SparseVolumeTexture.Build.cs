// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SparseVolumeTexture : ModuleRules
{
	public SparseVolumeTexture(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.Add(ModuleDirectory + "/Private");
		PublicIncludePaths.Add(ModuleDirectory + "/Public");

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Projects",
				"Engine",
				"Renderer",
				"RenderCore",
				"Slate",
				"SlateCore",
				"ApplicationCore",
				"ToolWidgets",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"AssetRegistry",
                "AssetTools",
				"EditorWidgets",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Renderer",
				"RenderCore",
				"RHI",
				"Core",
				"CoreUObject",
				"Engine",
                "InputCore",
                "Settings",
			}
		);

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] 
				{
					"MainFrame",
					"EditorFramework",
					"UnrealEd"
				}
			);
		}

		// Specific to OpenVDB support
		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
		{
			bUseRTTI = true;
			bEnableExceptions = true;

			AddEngineThirdPartyPrivateStaticDependencies(Target,
				"IntelTBB",
				"Blosc",
				"zlib",
				"Boost",
				"OpenVDB"
			);
		}
	}
}
