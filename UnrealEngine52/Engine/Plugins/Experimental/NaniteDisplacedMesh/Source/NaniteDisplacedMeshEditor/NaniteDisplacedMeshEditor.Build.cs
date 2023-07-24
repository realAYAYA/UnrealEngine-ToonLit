// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class NaniteDisplacedMeshEditor : ModuleRules
	{
		public NaniteDisplacedMeshEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.Add("NaniteDisplacedMeshEditor/Private");
			PublicIncludePaths.Add(ModuleDirectory + "/Public");

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"AssetRegistry",
					"AssetTools",
					"Core",
					"CoreUObject",
					"EditorFramework",
					"EditorStyle",
					"Engine",
					"PropertyEditor",
					"RHI",
					"Slate",
					"Slate",
					"SlateCore",
					"TargetPlatform",
					"UnrealEd",
					"SourceControl",
				}
			);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"EditorSubsystem",
					"NaniteDisplacedMesh"
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"DerivedDataCache",
				}
			);
		}
	}
}
