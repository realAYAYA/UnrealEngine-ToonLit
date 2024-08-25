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
					"AudioMixer",
					"SignalProcessing",
					"Projects"
				}
			);

			PublicDependencyModuleNames.AddRange
			(
				new string[]
				{
					"MetasoundGraphCore",
					"MetasoundGenerator",
					"MetasoundFrontend",
					"MetasoundEngine",
					"MetasoundStandardNodes"
				}
			);
		}
	}
}
