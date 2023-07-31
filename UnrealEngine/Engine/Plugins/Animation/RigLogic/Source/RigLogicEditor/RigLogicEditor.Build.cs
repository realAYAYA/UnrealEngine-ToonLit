// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
    public class RigLogicEditor : ModuleRules
    {
        private string ModulePath
        {
            get { return ModuleDirectory; }
        }

        public RigLogicEditor(ReadOnlyTargetRules Target) : base(Target)
        {
            if (Target.LinkType != TargetLinkType.Monolithic)
            {
                PublicDefinitions.Add("RL_SHARED=1");
            }

            PublicDependencyModuleNames.AddRange(
                new string[]
                {
                    "Core",
                    "CoreUObject",
                    "Engine",
                    "ControlRig",
                    "UnrealEd",
                    "EditorFramework",
                    "MainFrame",
                    "RigLogicModule",
                    "RigLogicLib",
                    "PropertyEditor",
                    "SlateCore",
                    "ApplicationCore",
                    "Slate",
                    "InputCore"
                }
            );

            PrivateIncludePathModuleNames.AddRange(
                new string[]
                {
                    "PropertyEditor",
                    "AssetTools"
                }
            );

        }
    }
}
