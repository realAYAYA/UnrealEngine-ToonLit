// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LiveLinkFaceImporter : ModuleRules
{
	public LiveLinkFaceImporter(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
				
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"LiveLink"
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"UnrealEd",
				"LevelSequence",
				"MovieScene",
				"MovieSceneTracks",
				"LiveLink",
				"LiveLinkInterface",
				"LiveLinkMovieScene"
			}
			);
		
	}
}
