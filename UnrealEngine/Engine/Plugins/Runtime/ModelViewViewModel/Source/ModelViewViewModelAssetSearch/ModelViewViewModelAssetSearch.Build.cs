// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using System.Collections.Generic;
using UnrealBuildTool;

public class ModelViewViewModelAssetSearch : ModuleRules 
{
	public ModelViewViewModelAssetSearch(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"AssetSearch",
				"Core",
				"CoreUObject",
				"ModelViewViewModel",
				"ModelViewViewModelBlueprint",
			});

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"InputCore",
				"UMGEditor",
			});
	}
}
