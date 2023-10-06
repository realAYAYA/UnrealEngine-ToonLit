// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NNEEditorTools : ModuleRules
{
	public NNEEditorTools(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange
			(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"NNE"
			}
		);

		if (Target.Type == TargetType.Editor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Slate",
					"SlateCore",
					"UnrealEd",
					"AssetTools"
				}
			);
		}
	}
}
