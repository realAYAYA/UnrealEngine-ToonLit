// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using UnrealBuildTool;

public class stb_image_resize2 : ModuleRules
{
	public stb_image_resize2(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicSystemIncludePaths.Add(ModuleDirectory);
	}
}

