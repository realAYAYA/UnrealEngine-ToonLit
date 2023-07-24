// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CustomizableObject : ModuleRules
{
	public CustomizableObject(ReadOnlyTargetRules TargetRules) : base(TargetRules)
	{
		ShortName = "MuCO";

		DefaultBuildSettings = BuildSettingsVersion.V2;

		bAllowConfidentialPlatformDefines = true;
		//bUseUnity = false;


		PrivateDependencyModuleNames.AddRange(new string[] {
            "InputCore",
            "SlateCore",
            "Slate",
            "Core",
            "CoreUObject",
            "Engine",
            "RenderCore",
            "RHI",
            "AppFramework",
            "Projects",
            "ApplicationCore",
			"ClothingSystemRuntimeCommon",
			"ClothingSystemRuntimeInterface",
			//"ClothingSystemEditor",
			"UMG",

			"MutableRuntime",
		});

        PublicDependencyModuleNames.AddRange(new string[] {
            "Slate",
			"Core",
            "Engine",
			"SkeletalMerging",
			"ClothingSystemRuntimeCommon",
            "GameplayTags",
		});

        PrivateIncludePathModuleNames.AddRange(
        new string[] {
                "TargetPlatform"
        });

		PrivateIncludePaths.AddRange(new string[] {
				"MutableRuntime/Private",
			});

		if (TargetRules.bBuildEditor == true)
        {
            PublicDependencyModuleNames.Add("UnrealEd");	// @todo api: Only public because of WITH_EDITOR
            PublicDependencyModuleNames.Add("DerivedDataCache");
            PublicDependencyModuleNames.Add("EditorStyle");
            PublicDependencyModuleNames.Add("MessageLog");
        }

    }
}
