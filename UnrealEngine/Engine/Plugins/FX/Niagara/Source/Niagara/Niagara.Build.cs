// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class Niagara : ModuleRules
{
    public Niagara(ReadOnlyTargetRules Target) : base(Target)
	{
		NumIncludedBytesPerUnityCPPOverride = 491520; // best unity size found from using UBT ProfileUnitySizes mode
		
		PrivateDependencyModuleNames.AddRange(
            new string[] {
                "ApplicationCore",
                "AudioPlatformConfiguration",
                "Core",
                "DeveloperSettings",
                "Engine",
                "Json",
                "JsonUtilities",
                "Landscape",
                "Renderer",
                "SignalProcessing",
                "TimeManagement",
                "TraceLog",
            }
        );


        PublicDependencyModuleNames.AddRange(
            new string[] {
                "CoreUObject",
                "IntelISPC",
                "MovieScene",
                "MovieSceneTracks",
                "NiagaraCore",
                "NiagaraShader",
                "NiagaraVertexFactories",
                "PhysicsCore",
                "RenderCore",
                "RHI",
                "VectorVM",
            }
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"DerivedDataCache",
				"Shaders"
			});

        if (Target.bBuildEditor == true)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[] {
                "TargetPlatform",
                "DerivedDataCache",
				"EditorFramework",
                "UnrealEd",
				"SlateCore",
				"Slate"
            });
        }

		PublicDefinitions.AddRange(
            new string[]
            {
                "VECTORVM_SUPPORTS_EXPERIMENTAL=1",
                "VECTORVM_SUPPORTS_LEGACY=1"
            });
	}
}
