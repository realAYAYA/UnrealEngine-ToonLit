// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MaterialAnalyzer : ModuleRules
	{
		public MaterialAnalyzer(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicIncludePathModuleNames.AddRange(
				new string[] {
				"WorkspaceMenuStructure"
				}
			);
			PublicDependencyModuleNames.AddRange(new string[] {
					"Core",
					"CoreUObject",
					"Engine",
					"InputCore",
					"Slate",
					"SlateCore",
					"EditorFramework",
					"UnrealEd",
					"PropertyEditor"
			});

            PrivateDependencyModuleNames.AddRange(new string[] {
                    "AssetManagerEditor"
            });
        }
	}

}