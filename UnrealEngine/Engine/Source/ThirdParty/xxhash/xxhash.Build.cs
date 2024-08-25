// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using UnrealBuildTool;

public class xxhash : ModuleRules
{
	public xxhash(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicSystemIncludePaths.Add(ModuleDirectory);
	}
}

