// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class FacialAnimation : ModuleRules
	{
		public FacialAnimation(ReadOnlyTargetRules Target) : base(Target)
		{			
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"InputCore",
					"Engine",
					"AudioExtensions",
					"AudioMixer"
				}
			);
		}
	}
}
