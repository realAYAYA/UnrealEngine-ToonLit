// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class DataflowCore : ModuleRules
	{
        public DataflowCore(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Chaos",
					"DeveloperSettings",
					"InputCore",
					"ApplicationCore"
				}
			);
		}
	}
}
