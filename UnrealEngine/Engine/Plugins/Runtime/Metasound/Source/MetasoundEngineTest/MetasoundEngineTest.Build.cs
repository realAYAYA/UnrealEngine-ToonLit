// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class MetasoundEngineTest : ModuleRules
	{
		public MetasoundEngineTest(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange
			(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"AudioExtensions",
					"MetasoundGraphCore",
					"MetasoundGenerator",
					"MetasoundFrontend",
					"MetasoundEngine",
					"SignalProcessing",
					"Projects"
				}
			);

			PublicDependencyModuleNames.AddRange
			(
				new string[]
				{
				}
			);
		}
	}
}
