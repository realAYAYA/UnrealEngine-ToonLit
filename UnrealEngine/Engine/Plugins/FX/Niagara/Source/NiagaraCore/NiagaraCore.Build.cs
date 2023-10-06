// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NiagaraCore : ModuleRules
{
    public NiagaraCore(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "Core",
            }
        );

        PublicDependencyModuleNames.AddRange(
            new string[] {
				"CoreUObject",
                "VectorVM",
            }
        );

		PublicIncludePathModuleNames.AddRange(
			new string[] {
				"RenderCore",
			}
		);
	}
}
