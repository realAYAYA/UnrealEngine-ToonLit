// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class GoogleOboe : ModuleRules
{
	public GoogleOboe(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "Core"
                }
            );

        Type = ModuleType.CPlusPlus;

            

        PublicSystemIncludePaths.Add(Target.UEThirdPartySourceDirectory + "GoogleOboe/Public");
        PublicDefinitions.Add("WITH_GOOGLEOBOE=1");
    }
}
