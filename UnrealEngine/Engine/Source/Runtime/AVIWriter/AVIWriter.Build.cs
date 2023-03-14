// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AVIWriter : ModuleRules
	{
		public AVIWriter(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
				}
				);

			if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
			{
				if (Target.bBuildDeveloperTools)
				{
					AddEngineThirdPartyPrivateStaticDependencies(Target, "DirectShow");
				}
			}
			else if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				PublicFrameworks.AddRange(new string[] { "AVFoundation", "CoreVideo", "CoreMedia" });
			}
		}
	}
}
