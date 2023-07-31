// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ImagePlateEditor : ModuleRules
	{
		public ImagePlateEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"AssetTools",
					"Core",
					"CoreUObject",
					"EditorFramework",
					"Engine",
					"ImagePlate",
					"RHI",
					"Slate",
					"SlateCore",
                    "TimeManagement",
					"UnrealEd",
				}
			);
		}
	}
}
