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

			PublicDependencyModuleNames.AddRange(
                new string[] {
					"MutableRuntime",
					"Core",
					"CoreUObject",
					"GeometryCore",
					"ImageCore",
					"TextureCompressor",
					"TextureBuildUtilities",
				}
			);
		}
	}
}
