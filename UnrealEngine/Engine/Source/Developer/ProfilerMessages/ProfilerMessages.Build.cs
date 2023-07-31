// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ProfilerMessages : ModuleRules
	{
		public ProfilerMessages(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"CoreUObject",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
				});

			if (Target.Configuration != UnrealTargetConfiguration.Shipping)
			{
				PrecompileForTargets = PrecompileTargetsType.Any;
			}
		}
	}
}
