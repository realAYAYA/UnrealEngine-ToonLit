// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class MetasoundFrontend : ModuleRules
	{
		public MetasoundFrontend(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange
			(
				new string[]
				{
					"AudioExtensions",
					"Core",
					"CoreUObject",
					"Serialization",
					"SignalProcessing",
					"MetasoundGraphCore"

				}
			);

			PrivateDependencyModuleNames.AddRange
			(
				new string[]
				{
				}
			);

			PublicDefinitions.Add("WITH_METASOUND_FRONTEND=1");
		}
	}
}
