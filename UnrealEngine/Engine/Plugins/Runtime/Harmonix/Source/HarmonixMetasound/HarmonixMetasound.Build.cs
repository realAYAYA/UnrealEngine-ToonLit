// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HarmonixMetasound : ModuleRules
{
	public HarmonixMetasound(ReadOnlyTargetRules Target) : base(Target)
	{
		//OptimizeCode = CodeOptimization.Never;

		// This next flag is needed as there are Metasound node derivatives 
		// that are implemented in .cpp files only... no header files. 
		IWYUSupport = IWYUSupport.None;

		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Engine",
				"Core",
				"CoreUObject",
				"MetasoundStandardNodes",
				"HarmonixDsp",
				"HarmonixMidi",
				"Harmonix"
			});
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AudioExtensions",
				"MetasoundEngine",
				"MetasoundGraphCore",
				"MetasoundFrontend",
				"MetasoundGenerator",
				"SignalProcessing"
			});

		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.Add("AssetRegistry");
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
