// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class IrisStub : ModuleRules
	{
        public IrisStub(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"NetCore",
                }
            );

			if (Target.bUseIris == true)
			{
				PublicDefinitions.Add("UE_WITH_IRIS=1");
				// This module should not be precompiled if Iris is enabled
				if (Target.bBuildAllModules == true)
				{
					PrecompileForTargets = PrecompileTargetsType.None;
				}
			}
			else
			{
				PublicDefinitions.Add("UE_WITH_IRIS=0");
			}

		}
	}
}
