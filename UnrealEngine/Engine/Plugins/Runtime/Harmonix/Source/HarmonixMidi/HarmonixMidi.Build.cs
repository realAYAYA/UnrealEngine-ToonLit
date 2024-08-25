// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HarmonixMidi : ModuleRules
{
	public HarmonixMidi(ReadOnlyTargetRules Target) : base(Target)
	{
		//OptimizeCode = CodeOptimization.Never;

		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
			});
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AudioExtensions",
				"Harmonix",
			});
		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.Add("AssetRegistry");
			PrivateDependencyModuleNames.Add("UnrealEd");
			PrivateDependencyModuleNames.Add("Engine");
		}
		else
		{
			// This next little gem is needed because when this module is built without
			// an Engine dependency the MidiFile.gen.cpp file has an UNUSED forward declaration
			// at the top of it that is prefixed by "ENGINE_API", so the compiler sees an undefined
			// symbol. :-( - Buzz
			PrivateDefinitions.Add("ENGINE_API=");
		}
	}
}
