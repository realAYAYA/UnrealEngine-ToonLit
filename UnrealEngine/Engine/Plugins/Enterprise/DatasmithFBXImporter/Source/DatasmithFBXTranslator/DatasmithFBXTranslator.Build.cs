// Copyright Epic Games, Inc. All Rights Reserved.
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class DatasmithFBXTranslator : ModuleRules
	{
		public DatasmithFBXTranslator(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"DatasmithCore",
					"EditorFramework",
					"Engine",
					"LevelSequence",
					"MeshDescription",
					"StaticMeshDescription",
					"UnrealEd", // For UnFbx
				}
			);
			AddEngineThirdPartyPrivateStaticDependencies(Target,
				"FBX"
			);
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"DatasmithContent",
					"DatasmithTranslator"
				}
			);
		}
	}
}
