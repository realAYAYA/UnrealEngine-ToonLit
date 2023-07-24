// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class MetasoundGenerator : ModuleRules
	{
        public MetasoundGenerator(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"SignalProcessing",
					"CoreUObject",
					"Engine",
					"AudioExtensions",
					"AudioMixerCore"
				}
			);

            PublicDependencyModuleNames.AddRange(
				new string[] {
                    "Core",
					"AudioMixer",
					"MetasoundGraphCore",
					"MetasoundStandardNodes",
					"MetasoundFrontend"
                }
            );
		}
	}
}
