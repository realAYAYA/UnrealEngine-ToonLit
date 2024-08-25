// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class OXRVisionOS : ModuleRules
	{
        public OXRVisionOS(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"RenderCore",
				"RHI",
				"MetalRHI",
				"OpenXR",
				"OpenXRHMD",
				"OXRVisionOSSettings",
			});
			
			PublicWeakFrameworks.Add("Metal");
			PublicWeakFrameworks.Add("ARKit");		
			PrivateIncludePathModuleNames.Add("OpenXRHMD");
		}
	}
}
