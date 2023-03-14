// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class VisualStudioSetup : ModuleRules
{
	public VisualStudioSetup(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;
		
		if(Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicIncludePaths.Add("$(ModuleDir)/include");
			PublicAdditionalLibraries.Add("$(ModuleDir)/v141/x64/Microsoft.VisualStudio.Setup.Configuration.Native.lib");
		}
	}
}
