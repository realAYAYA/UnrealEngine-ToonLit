// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class LowLevelTestsRunner : ModuleRules
	{
		public LowLevelTestsRunner(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = PCHUsageMode.NoPCHs;
			PrecompileForTargets = PrecompileTargetsType.None;
			bUseUnity = false;
			bRequiresImplementModule = false;

			PublicIncludePaths.AddRange(
				new string[]
				{
					Path.Combine(Target.UEThirdPartySourceDirectory, "Catch2", "v3.4.0", "src")
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core"
				}
			);

			if (TestTargetRules.bTestsRequireEditor)
			{
				PrivateDependencyModuleNames.Add("UnrealEd");
			}
			if (TestTargetRules.bTestsRequireEngine)
			{
				PrivateDependencyModuleNames.Add("Engine");
			}
			if (TestTargetRules.bTestsRequireApplicationCore)
			{
				PrivateDependencyModuleNames.Add("ApplicationCore");
			}
			if (TestTargetRules.bTestsRequireCoreUObject)
			{
				PrivateDependencyModuleNames.Add("CoreUObject");
			}
		}
	}
}
