// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MaterialAnalyzer : ModuleRules
	{
		public MaterialAnalyzer(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicIncludePaths.AddRange(
				new string[] {
				"Editor/WorkspaceMenuStructure/Public"
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