// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO; // for Path
public class HairCardGeneratorEditor : ModuleRules
{
	public HairCardGeneratorEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"HairCardGeneratorFramework",
				// ... add other public dependencies that you statically link with here ...
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"HairStrandsCore", // for FHairDescription
				"MeshDescription",
				"StaticMeshDescription",
				"AssetRegistry",
				"UnrealEd",
				"Slate",
				"SlateCore",
				"EditorStyle",
				"MainFrame",
				"ApplicationCore",
				"InputCore",
				"ToolWidgets",
				"Eigen",
				"Json",
				"JsonUtilities",
				// ... add private dependencies that you statically link with here ...	
			}
		);
	}
}
