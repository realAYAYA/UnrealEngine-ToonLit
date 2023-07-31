// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MfMediaFactory : ModuleRules
	{
		public MfMediaFactory(ReadOnlyTargetRules Target) : base(Target)
		{
			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"Media",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"MediaAssets",
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Media",
					"MfMedia",
				});

			PrivateIncludePaths.AddRange(
				new string[] {
					"MfMediaFactory/Private",
				});

			if (DoAllowHTTPSPlayback())
			{
				PrivateDefinitions.Add("MFMEDIAFACTORY_ALLOW_HTTPS=1");
			}
			else
			{
				PrivateDefinitions.Add("MFMEDIAFACTORY_ALLOW_HTTPS=0");
			}
		}

		protected virtual bool DoAllowHTTPSPlayback()
		{
			return Target.Platform.IsInGroup(UnrealPlatformGroup.Windows);
		}
	}
}
