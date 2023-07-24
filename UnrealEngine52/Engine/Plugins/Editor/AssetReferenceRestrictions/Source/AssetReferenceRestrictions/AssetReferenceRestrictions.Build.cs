// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AssetReferenceRestrictions : ModuleRules
	{
        public AssetReferenceRestrictions(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
                {
                    "Core",
                    "CoreUObject",
                }
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"DeveloperSettings",
					"DeveloperToolSettings",
					"Engine",
					"EditorSubsystem",
					"UnrealEd",
					"EditorFramework",
					"Projects",
					"DataValidation"
				}
			);
		}
	}
}
