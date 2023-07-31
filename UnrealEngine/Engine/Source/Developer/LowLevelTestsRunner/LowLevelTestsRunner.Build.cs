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
				"Runtime/Launch/Public",
				Path.Combine(Target.UEThirdPartySourceDirectory, "Catch2", "v3.0.1", "src")
				}
			);

			PrivateIncludePaths.AddRange(
				new string[] {
				"Runtime/Launch/Private"
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[] {
				"Core",
				"Projects"
				}
			);
		}
	}
}
