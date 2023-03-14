// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Cascade : ModuleRules
{
	public Cascade(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.AddRange(
			new string[] {
				"Editor/DistCurveEditor/Public",
				"Editor/UnrealEd/Public",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
                "AppFramework",
				"Core",
				"CoreUObject",
                "InputCore",
				"Engine",
				"Slate",
				"SlateCore",
				"DistCurveEditor",
				"EditorFramework",
				"UnrealEd",
				"RHI",
				"RenderCore",
				"PhysicsCore"
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"MainFrame",
				"PropertyEditor"
			}
		);
	}
}
