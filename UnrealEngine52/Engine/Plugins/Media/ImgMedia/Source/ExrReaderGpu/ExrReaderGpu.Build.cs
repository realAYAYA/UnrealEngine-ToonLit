// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ExrReaderGpu : ModuleRules
	{
		public ExrReaderGpu(ReadOnlyTargetRules Target) : base(Target)
		{
			bEnableExceptions = true;
			bUseRTTI = true;

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"RenderCore",
					"Projects",
					"Engine",
				});

			if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
			{
				AddEngineThirdPartyPrivateStaticDependencies(Target, "Imath");
				AddEngineThirdPartyPrivateStaticDependencies(Target, "UEOpenExr");
				PrivateDependencyModuleNames.Add("OpenExrWrapper");
				PrivateIncludePaths.Add("ExrReaderGpu/Private");
			}

		}
	}
}
