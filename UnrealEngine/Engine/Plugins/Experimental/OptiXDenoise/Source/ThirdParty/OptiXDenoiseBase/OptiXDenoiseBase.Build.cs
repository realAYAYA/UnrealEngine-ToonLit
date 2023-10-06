// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class OptiXDenoiseBase : ModuleRules
{
    public OptiXDenoiseBase(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;

        if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows) && Target.WindowsPlatform.Architecture == UnrealArch.X64)
        {
			PublicSystemIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));
			PublicDelayLoadDLLs.Add("OptiXDenoiseBase.dll");
			PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "lib", "OptiXDenoiseBase.lib"));
		}
		else
		{
			
		}
    }
}
