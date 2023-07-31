// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class ChaosVehiclesCore : ModuleRules
    {
        public ChaosVehiclesCore(ReadOnlyTargetRules Target) : base(Target)
        {
            PublicIncludePaths.Add(ModuleDirectory + "/Public");

            PublicDependencyModuleNames.AddRange(
                new string[] {
                "Core",
                "CoreUObject",
				"Chaos"
				}
            ); 

			PrivateDefinitions.Add("CHAOS_INCLUDE_LEVEL_1=1");
		}
    }
}
