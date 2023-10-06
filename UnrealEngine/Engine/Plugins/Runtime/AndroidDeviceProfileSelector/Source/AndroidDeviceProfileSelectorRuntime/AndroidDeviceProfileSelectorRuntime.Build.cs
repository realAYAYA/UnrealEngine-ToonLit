// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class AndroidDeviceProfileSelectorRuntime : ModuleRules
	{
        public AndroidDeviceProfileSelectorRuntime(ReadOnlyTargetRules Target) : base(Target)
		{
			ShortName = "AndroidDPSRT";

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
				    "CoreUObject",
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
				    "Core",
				    "CoreUObject",
				    "Engine",
					"AndroidDeviceProfileSelector",
					"HeadMountedDisplay",
				}
				);
		}
	}
}
