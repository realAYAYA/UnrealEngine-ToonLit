// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class ControlRig : ModuleRules
    {
        public ControlRig(ReadOnlyTargetRules Target) : base(Target)
        {
            PrivateIncludePaths.Add("ControlRig/ThirdParty/AHEasing");

            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "Core",
                    "CoreUObject",
                    "Engine",
                    "AnimGraphRuntime",
                    "MovieScene",
                    "MovieSceneTracks",
                    "PropertyPath",
					"TimeManagement",
					"DeveloperSettings"
				}
            );

            PublicDependencyModuleNames.AddRange(
                new string[]
                {
                    "AnimationCore",
                    "LevelSequence",
                    "RigVM",
                    "RHI",
                    "Constraints"
                }
            );

            if (Target.bBuildEditor == true)
            {
                PublicDependencyModuleNames.AddRange(
				    new string[]
					{
						"Slate",
						"SlateCore",
						"RigVMDeveloper",
                        "AnimGraph",
                        "Json",
                        "Serialization",
                        "JsonUtilities",
                        "AnimationBlueprintLibrary",
                    }
                );

                PrivateDependencyModuleNames.AddRange(
                    new string[]
                    {
						"EditorFramework",
                        "UnrealEd",
                        "BlueprintGraph",
                        "PropertyEditor",
                        "RigVMDeveloper",
                    }
                );

                PrivateIncludePathModuleNames.Add("ControlRigEditor");
                DynamicallyLoadedModuleNames.Add("ControlRigEditor");
            }
        }
    }
}
