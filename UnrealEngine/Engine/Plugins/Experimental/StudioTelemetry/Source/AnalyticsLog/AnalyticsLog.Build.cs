// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AnalyticsLog : ModuleRules
	{
		public AnalyticsLog(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
						"Core"
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[] {
				"CoreUObject",
				"Analytics"
				}
			);
		}
	}
}
