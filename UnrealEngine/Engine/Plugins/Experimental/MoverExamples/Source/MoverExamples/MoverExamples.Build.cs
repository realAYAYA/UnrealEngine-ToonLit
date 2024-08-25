// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MoverExamples : ModuleRules
{
	public MoverExamples(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] 
			{ 
				"Core", 
				"CoreUObject", 
				"Engine", 
				"InputCore", 
				"Mover",
				"EnhancedInput"
			});
	}
}
