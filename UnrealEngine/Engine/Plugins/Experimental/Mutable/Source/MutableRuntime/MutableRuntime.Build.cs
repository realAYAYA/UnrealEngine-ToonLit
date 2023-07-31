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
			//bUseUnity = false;

			PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));

			PublicDependencyModuleNames.AddRange(
				new string[] {
				"Core"
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
