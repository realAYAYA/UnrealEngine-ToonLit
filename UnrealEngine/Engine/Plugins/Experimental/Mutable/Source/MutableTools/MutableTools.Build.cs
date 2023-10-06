// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
    public class MutableTools : ModuleRules
    {
        public MutableTools(ReadOnlyTargetRules Target) : base(Target)
		{
			ShortName = "MuT";

			DefaultBuildSettings = BuildSettingsVersion.V2;
			IWYUSupport = IWYUSupport.KeepAsIsForNow;
			//bUseUnity = false;

			// 
			bUseRTTI = true;
			bEnableExceptions = true;

			PrivateIncludePaths.AddRange(new string[] {
				System.IO.Path.Combine(GetModuleDirectory("MutableRuntime"), "Private"),
			});

			PublicDependencyModuleNames.AddRange(
                new string[] {
					"MutableRuntime", 
					"Core",
					"GeometryCore",
				}
            );
		}
	}
}
