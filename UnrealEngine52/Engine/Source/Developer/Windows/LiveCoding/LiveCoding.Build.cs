// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class LiveCoding : ModuleRules
{
	public LiveCoding(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.Add("Core");
		PrivateDependencyModuleNames.Add("CoreUObject");
		PrivateDependencyModuleNames.Add("Settings");

		if (Target.bCompileAgainstEngine)
		{
			PrivateDependencyModuleNames.Add("Engine");
		}

		if (Target.bBuildEditor == true)
        {
            PrivateDependencyModuleNames.Add("UnrealEd");
			PrivateDependencyModuleNames.Add("Slate");
		}

		if(Target.bUseDebugLiveCodingConsole)
        {
            PrivateDefinitions.Add("USE_DEBUG_LIVE_CODING_CONSOLE=1");
        }
		else
        {
            PrivateDefinitions.Add("USE_DEBUG_LIVE_CODING_CONSOLE=0");
        }

        if (Target.Configuration == UnrealTargetConfiguration.Debug)
        {
        	PrivateDefinitions.Add("LC_DEBUG=1");
        }
        else
        {
        	PrivateDefinitions.Add("LC_DEBUG=0");
        }

		PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "Private", "External", "LC_JumpToSelf.lib"));
	}
}
