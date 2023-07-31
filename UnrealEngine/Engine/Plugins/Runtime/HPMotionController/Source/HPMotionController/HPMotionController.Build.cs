// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

namespace UnrealBuildTool.Rules
{
    public class HPMotionController : ModuleRules
    {
        public HPMotionController(ReadOnlyTargetRules Target) 
				: base(Target)
        {
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
                    "InputCore",
					"OpenXRHMD",
				}
				);

            AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenXR");
        }
    }
}
