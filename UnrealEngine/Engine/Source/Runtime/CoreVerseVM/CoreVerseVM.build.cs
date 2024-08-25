// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CoreVerseVM : ModuleRules
{
	public CoreVerseVM(ReadOnlyTargetRules Target) : base(Target)
	{
		FPSemantics = FPSemanticsMode.Precise;
		PCHUsage = PCHUsageMode.NoPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
		);

		bAllowAutoRTFMInstrumentation = true;
	}
}
