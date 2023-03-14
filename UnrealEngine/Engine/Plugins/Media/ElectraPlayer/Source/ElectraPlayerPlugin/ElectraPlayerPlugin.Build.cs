// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

namespace UnrealBuildTool.Rules
{
	public class ElectraPlayerPlugin: ModuleRules
	{
		public ElectraPlayerPlugin(ReadOnlyTargetRules Target) : base(Target)
		{
			bLegalToDistributeObjectCode = true;

			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"Media",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"MediaUtils",
					"RHI",
					"AnalyticsET",
					"ElectraPlayerFactory",
					"ElectraPlayerRuntime",
					"ElectraSamples",
					"ElectraBase"
				});

			PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Media",
			});

			if (Target.bCompileAgainstEngine)
			{
				PrivateDependencyModuleNames.Add("Engine");
			}

			if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				PrivateDependencyModuleNames.Add("MetalRHI");
			}

			if (Target.Platform == UnrealTargetPlatform.IOS )
			{
				PrivateDependencyModuleNames.Add("MetalRHI");
			}

			if (Target.Type == TargetType.Editor)
			{
				PrivateDependencyModuleNames.Add("DeveloperSettings");
			}
		}
	}
}
