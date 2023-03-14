// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class MetasoundEngine : ModuleRules
	{
		public MetasoundEngine(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePathModuleNames.AddRange
			(
				new string[]
				{
					"AVEncoder",
				}
			);

			PrivateDependencyModuleNames.AddRange
			(
				new string[]
				{
					"Core",
					"AudioExtensions",
					"AudioCodecEngine",
					"MetasoundGraphCore",
					"MetasoundGenerator",
					"SignalProcessing"
				}
			);

			PublicDependencyModuleNames.AddRange
			(
				new string[]
				{
					"CoreUObject",
					"DeveloperSettings",
					"Engine",
					"MetasoundFrontend",
					"MetasoundStandardNodes",
					"AudioCodecEngine",
					"Serialization",
					"AudioPlatformConfiguration",
					"WaveTable"
				}
			);
		}
	}
}
