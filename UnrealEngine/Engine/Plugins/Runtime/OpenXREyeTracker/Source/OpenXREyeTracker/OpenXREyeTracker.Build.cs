// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class OpenXREyeTracker : ModuleRules
    {
        public OpenXREyeTracker(ReadOnlyTargetRules Target) 
				: base(Target)
        {
			PublicDependencyModuleNames.Add("EyeTracker");

            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "Core",
                    "CoreUObject",
					"ApplicationCore",
                    "Engine",
                    "InputDevice",
                    "InputCore",
                    "HeadMountedDisplay",
                    "OpenXRHMD",
					"OpenXRInput",
					"XRBase"
				}
				);

            AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenXR");

            if (Target.bBuildEditor == true)
            {
                PrivateDependencyModuleNames.Add("UnrealEd");
            }
        }
    }
}
