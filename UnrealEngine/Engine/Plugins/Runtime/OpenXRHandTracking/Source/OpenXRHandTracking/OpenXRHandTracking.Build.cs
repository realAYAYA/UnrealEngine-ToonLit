// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

namespace UnrealBuildTool.Rules
{
    public class OpenXRHandTracking: ModuleRules
    {
        public OpenXRHandTracking(ReadOnlyTargetRules Target) 
				: base(Target)
        {
			PublicDependencyModuleNames.AddRange(
			   new string[]
			   {
					"InputDevice"
			   }
		   );

			var EngineDir = Path.GetFullPath(Target.RelativeEnginePath);
            PrivateIncludePaths.AddRange(
                new string[] {
                    EngineDir + "/Source/ThirdParty/OpenXR/include"
				}
                );

            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "Core",
                    "CoreUObject",
                    "Engine",
					"HeadMountedDisplay",
                    "InputCore",
					"LiveLinkAnimationCore",
					"LiveLinkInterface",
					"OpenXRHMD",
					"OpenXRInput",
					"Slate",
					"SlateCore",
					"ApplicationCore"
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
