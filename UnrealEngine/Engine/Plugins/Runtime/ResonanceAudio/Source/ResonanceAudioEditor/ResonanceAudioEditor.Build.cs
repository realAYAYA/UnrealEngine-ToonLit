//
// Copyright (C) Google Inc. 2017. All rights reserved.
//

namespace UnrealBuildTool.Rules
{
    public class ResonanceAudioEditor : ModuleRules
    {
        public ResonanceAudioEditor(ReadOnlyTargetRules Target) : base(Target)
        {
            PrivateIncludePaths.AddRange(
                new string[] {
                    "ResonanceAudioEditor/Private",
                    "ResonanceAudio/Private",
                    "ResonanceAudio/Private/ResonanceAudioLibrary/resonance_audio",
                    "ResonanceAudio/Private/ResonanceAudioLibrary"
                }
            );

            PublicIncludePaths.AddRange(
                new string[] {
                }
            );


            PublicDependencyModuleNames.AddRange(
                new string[] {
                    "Core",
                    "CoreUObject",
                    "Engine",
                    "InputCore",
                    "UnrealEd",
                    "LevelEditor",
                    
                    "RenderCore",
                    "RHI",
                    "AudioEditor",
                    "AudioMixer",
                    "ResonanceAudio"
                }
            );

            PrivateIncludePathModuleNames.AddRange(
                new string[] {
                    "AssetTools",
                    "Landscape"
            });

            PrivateDependencyModuleNames.AddRange(
                new string[] {
                    "Slate",
                    "SlateCore",
					"EditorFramework",
                    "UnrealEd",
                    "AudioEditor",
                    "LevelEditor",
                    "Landscape",
                    "Core",
                    "CoreUObject",
                    "Engine",
                    "InputCore",
                    "PropertyEditor",
                    "Projects",
                    
                    "ResonanceAudio",
					"Eigen"
                 }
            );
        }
    }
}
