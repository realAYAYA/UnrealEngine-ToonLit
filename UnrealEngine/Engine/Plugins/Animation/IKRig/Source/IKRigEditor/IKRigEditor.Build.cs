// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
    public class IKRigEditor : ModuleRules
    {
        public IKRigEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.AddRange(
				new string[] {
					System.IO.Path.Combine(GetModuleDirectory("Persona"), "Private"),
				}
			);

			PublicDependencyModuleNames.AddRange(
                new string[]
                {
	                "Core",
	                "AdvancedPreviewScene",
                }
            );

			PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "InputCore",
                    "EditorFramework",
                    "UnrealEd",
                    "ToolMenus",
                    "CoreUObject",
                    "Engine",
                    "Slate",
                    "SlateCore",
                    "AssetTools",
                    "EditorWidgets",
                    "Kismet",
                    "KismetWidgets",
                    "Persona",
                    "SkeletonEditor",
                    "ContentBrowser",
                    "AnimationBlueprintLibrary",
                    "MessageLog",
                    
					"PropertyEditor",
					"BlueprintGraph",
					"AnimGraph",
					"AnimGraphRuntime",
					
					"IKRig",
					"IKRigDeveloper",
					"ToolWidgets",
					"AnimationCore",
                    "AnimationWidgets",
                    "ApplicationCore",
                }
            );
        }
    }
}
