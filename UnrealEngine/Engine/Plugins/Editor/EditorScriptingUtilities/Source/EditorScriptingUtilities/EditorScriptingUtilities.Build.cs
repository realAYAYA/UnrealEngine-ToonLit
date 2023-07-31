// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

namespace UnrealBuildTool.Rules
{
	public class EditorScriptingUtilities : ModuleRules
	{
		public EditorScriptingUtilities(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.AddRange(
				new string[] {
					System.IO.Path.Combine(GetModuleDirectory("UnrealEd"), "Private"),
				}
			);

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"AssetRegistry",
					"Core",
					"CoreUObject",
					"Engine",
					"StaticMeshEditor",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"AssetTools",
					"MainFrame",
					"MeshDescription",
					"RawMesh",
					"Slate",
					"SlateCore",
					"StaticMeshDescription",
					"EditorFramework",
					"UnrealEd",
                    "SkeletalMeshUtilitiesCommon",
                    "LevelEditor",
					"SkeletalMeshEditor",
					"PhysicsCore",
					"PhysicsUtilities",
                }
            );
		}
	}
}
