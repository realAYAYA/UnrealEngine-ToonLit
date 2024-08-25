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
				"FieldNotification",
				"ModelViewViewModel",
				"StructUtils",
			});

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"BlueprintGraph",
				"Kismet",
				"KismetCompiler",
				"PropertyEditor",
				"PropertyPath",
				"SlateCore",
				"Slate",
				"UMG",
				"UMGEditor",
				"UnrealEd",
			});
	}
}
