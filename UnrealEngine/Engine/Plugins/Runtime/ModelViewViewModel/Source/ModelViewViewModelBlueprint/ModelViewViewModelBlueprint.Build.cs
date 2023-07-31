// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using System.Collections.Generic;
using UnrealBuildTool;

public class ModelViewViewModelBlueprint : ModuleRules 
{
	public ModelViewViewModelBlueprint(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"DeveloperSettings",
				"Engine",
				"ModelViewViewModel",
			});

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"BlueprintGraph",
				"KismetCompiler",
				"SlateCore",
				"Slate",
				"UMG",
				"UMGEditor",
				"UnrealEd",
			});

		PrivateIncludePaths.AddRange(
			new string[]
			{
				"ModelViewViewModel/Private",
			});
	}
}
