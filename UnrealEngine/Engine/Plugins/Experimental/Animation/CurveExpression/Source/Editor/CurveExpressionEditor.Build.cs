// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CurveExpressionEditor : ModuleRules
{
	public CurveExpressionEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AnimGraph",
				"BlueprintGraph",
				"Core",
				"CoreUObject",
				"CurveExpression",
				"Engine",
				"KismetCompiler",
				"Slate",
				"SlateCore",
				"UnrealEd",
			}
		);
	}
}
