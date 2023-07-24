// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

namespace UnrealBuildTool.Rules
{
    public class OpenXRViveTracker: ModuleRules
    {
        public OpenXRViveTracker(ReadOnlyTargetRules Target) 
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
                    EngineDir + "/Source/ThirdParty/OpenXR/include",
					EngineDir + "/Plugins/Runtime/OpenXR/Source/OpenXRHMD/Private",
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
