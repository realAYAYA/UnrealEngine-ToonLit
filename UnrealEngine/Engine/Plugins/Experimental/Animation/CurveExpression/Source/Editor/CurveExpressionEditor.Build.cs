// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using UnrealBuildTool;

public class CurveExpressionEditor : ModuleRules
{
	public CurveExpressionEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddAll(
			"AnimGraph",
			"AssetDefinition",
			"BlueprintGraph",
			"Core",
			"CoreUObject",
			"CurveExpression",
			"Engine", 
			"Kismet",
			"KismetCompiler",
			"Slate",
			"SlateCore",
			"UnrealEd"
		);
	}
}
