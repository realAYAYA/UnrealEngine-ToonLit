// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LandscapePatchEditorOnly : ModuleRules
{
	public LandscapePatchEditorOnly(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Engine",
				"LandscapePatch",
				"Slate",
				"SlateCore",
				"UnrealEd",
			}
			);
	}
}
