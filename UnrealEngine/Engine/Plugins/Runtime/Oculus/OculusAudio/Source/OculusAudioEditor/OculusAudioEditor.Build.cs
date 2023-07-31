// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class OculusAudioEditor : ModuleRules
    {
        public OculusAudioEditor(ReadOnlyTargetRules Target) : base(Target)
        {
            PublicDependencyModuleNames.AddRange(
                new string[] {
                "Core",
                "CoreUObject",
                "Engine",
                }
                );

            PrivateIncludePaths.AddRange(
                new string[] {
                    "OculusAudioEditor/Public",
                    "OculusAudio/Private",
                    "OculusAudio/Public",
 					// ... add other private include paths required here ...
				}
                );

            PrivateIncludePathModuleNames.AddRange(
            new string[] {
                "AssetTools",
                "AudioEditor",
                "OculusAudio",
                "UnrealEd",
                }
                );
            
            PublicDependencyModuleNames.AddRange(
                new string[] {
                    "Core",
                    "CoreUObject",
                    "Engine",
                    "InputCore",
					"EditorFramework",
                    "UnrealEd",
                    "RHI",
                    "AudioEditor",
                    "AudioMixer",
                    "OculusAudio"
                }
            );
        }
    }
}