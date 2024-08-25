// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NNERuntimeIREE : ModuleRules
{
	public NNERuntimeIREE(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange
		(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"IREE",
				"NNE",
				"Projects"
			}
		);

		if (Target.Type == TargetType.Editor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"DerivedDataCache",
					"Json",
					"TargetPlatform"
				}
			);
		}

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicDefinitions.Add("WITH_NNE_RUNTIME_IREE");
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			PublicDefinitions.Add("WITH_NNE_RUNTIME_IREE");
			PrivateDefinitions.Add("NNE_RUNTIME_IREE_USE_COMBINED_LIB_PATH");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicDefinitions.Add("WITH_NNE_RUNTIME_IREE");
			PrivateDefinitions.Add("NNE_RUNTIME_IREE_USE_COMBINED_LIB_PATH");
		}
	}
}
