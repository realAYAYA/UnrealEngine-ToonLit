// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class BlendSpaceMotionAnalysis : ModuleRules
	{
		public BlendSpaceMotionAnalysis(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"Persona",
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[]
				{
				}
			);

		}
	}
}
