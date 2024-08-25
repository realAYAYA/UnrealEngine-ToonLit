// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class IrisCore : ModuleRules
	{
        public IrisCore(ReadOnlyTargetRules Target) : base(Target)
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
			}
			else
			{
				PublicDefinitions.Add("UE_WITH_IRIS=0");

				// This module should not be precompiled if Iris is disabled
				if (Target.bBuildAllModules == true)
				{
					PrecompileForTargets = PrecompileTargetsType.None;
				}
			}

			bAllowAutoRTFMInstrumentation = true;

			UnsafeTypeCastWarningLevel = WarningLevel.Error;
		}
	}
}
