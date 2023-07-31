// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class SoundscapeEditor : ModuleRules
	{
		public SoundscapeEditor(ReadOnlyTargetRules Target) : base(Target)
        {
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
				"Core",
				"AssetTools",
				"Soundscape",
				"AudioMixer",

					// ... add other public dependencies that you statically link with here ...
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
					// ... add private dependencies that you statically link with here ...	
				}
				);
		}
	}
}