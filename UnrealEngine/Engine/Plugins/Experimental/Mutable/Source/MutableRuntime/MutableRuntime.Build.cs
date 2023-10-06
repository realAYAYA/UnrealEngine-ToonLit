// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
    public class MutableRuntime : ModuleRules
    {
        public MutableRuntime(ReadOnlyTargetRules Target) : base(Target)
		{
			ShortName = "MuR";

			DefaultBuildSettings = BuildSettingsVersion.V2;
			IWYUSupport = IWYUSupport.KeepAsIsForNow;
			//bUseUnity = false;

			PublicDependencyModuleNames.AddRange(
				new string[] {
				"Core",
				"CoreUObject",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[] {
				"GeometryCore",
				"TraceLog"
				}
			);
		}
	}
}
