// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class ElectraPlayerFactory : ModuleRules
    {
        public ElectraPlayerFactory(ReadOnlyTargetRules Target) : base(Target)
        {
		    bLegalToDistributeObjectCode = true;

            bool bSupportedPlatform = IsSupportedPlatform(Target);

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

            PrivateIncludePathModuleNames.Add("Media");

            if (bSupportedPlatform)
            {
                PrivateIncludePathModuleNames.Add("ElectraPlayerPlugin");
                PublicDefinitions.Add("UE_PLATFORM_ELECTRAPLAYER=1");
            }

            PrivateIncludePaths.AddRange(
                new string[] {
                    "ElectraPlayerFactory/Private",
                });

            PublicDependencyModuleNames.AddRange(
                new string[] {
                    "Core",
                    "CoreUObject",
                });

            if (Target.Type == TargetType.Editor)
            {
                DynamicallyLoadedModuleNames.Add("Settings");
                PrivateIncludePathModuleNames.Add("Settings");
            }

            if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
            {
                PublicDefinitions.Add("_CRT_SECURE_NO_WARNINGS");
            }

            if (bSupportedPlatform)
            {
                DynamicallyLoadedModuleNames.Add("ElectraPlayerPlugin");
            }
        }

		protected virtual bool IsSupportedPlatform(ReadOnlyTargetRules Target)
        {
			return Target.IsInPlatformGroup(UnrealPlatformGroup.Windows)
				|| Target.Platform == UnrealTargetPlatform.Mac
				|| Target.Platform == UnrealTargetPlatform.IOS
				|| Target.IsInPlatformGroup(UnrealPlatformGroup.Android)
				|| Target.IsInPlatformGroup(UnrealPlatformGroup.Unix)
				;
		}
    }
}
