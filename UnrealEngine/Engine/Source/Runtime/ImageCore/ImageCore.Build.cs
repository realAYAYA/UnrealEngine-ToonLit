// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ImageCore : ModuleRules
{
	public ImageCore(ReadOnlyTargetRules Target) : base(Target)
	{
		// include only, no link :
		// for TextureDefines.h :
		PrivateIncludePathModuleNames.AddRange(new string[]
		{
			"Engine",
		});

		PrivateDependencyModuleNames.Add("ColorManagement");
		PrivateDependencyModuleNames.Add("CoreUObject"); // for TextureDefines.h

		PublicDependencyModuleNames.Add("Core");
	}
}
