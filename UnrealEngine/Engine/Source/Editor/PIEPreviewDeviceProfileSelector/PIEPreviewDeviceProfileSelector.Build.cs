// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class PIEPreviewDeviceProfileSelector : ModuleRules
	{
        public PIEPreviewDeviceProfileSelector(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePathModuleNames.Add("Engine");

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
				    "CoreUObject",
                    "Json",
                }
                );

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
                    "PIEPreviewDeviceSpecification",
                    "RHI",
					"JsonUtilities",
					"MaterialShaderQualitySettings",
					"Slate",
					"SlateCore",
                    "ApplicationCore",
					"Engine",
					"EditorFramework",
					"UnrealEd",
                }
			);

			CircularlyReferencedDependentModules.AddRange(
				new string[]
				{
					"MaterialShaderQualitySettings",
					"UnrealEd"
				}
			);
		}
	}
}
