// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class DatasmithDeltaGenTranslator : ModuleRules
	{
		public DatasmithDeltaGenTranslator(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"DatasmithCore",
					"EditorFramework",
					"Engine",
					"FBX",
					"LevelSequence",
					"MeshDescription",
					"StaticMeshDescription",
					"UnrealEd",
					"XmlParser",
				}
			);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"DatasmithContent",
					"DatasmithTranslator",
                    "DatasmithFBXTranslator"
				}
			);

			//TODO: set this false by default and add option to turn this on for development
			PrivateDefinitions.Add("WITH_DELTAGEN_DEBUG_CODE=1");
		}
	}
}
