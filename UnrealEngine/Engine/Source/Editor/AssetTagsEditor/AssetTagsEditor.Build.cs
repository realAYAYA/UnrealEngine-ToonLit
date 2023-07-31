// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AssetTagsEditor : ModuleRules
{
	public AssetTagsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
            new string[] {
                "Core",
				"CoreUObject",
				"Slate",
				"SlateCore",
				
			}
        );
	}
}
