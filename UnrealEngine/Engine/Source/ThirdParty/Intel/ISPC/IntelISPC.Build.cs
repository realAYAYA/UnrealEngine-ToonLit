// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class IntelISPC : ModuleRules
{
	public IntelISPC(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.bCompileISPC == true)
        {
            // For Android, ISPC is on for some archs, off for others. Decide which in the tool chain.
            if (Target.Platform != UnrealTargetPlatform.Android)
            {
                PublicDefinitions.Add("INTEL_ISPC=1");
            }
        }
		else
        {
            PublicDefinitions.Add("INTEL_ISPC=0");
        }
	}
}