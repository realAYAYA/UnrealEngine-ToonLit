// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using UnrealBuildTool;

public class AtomicQueue : ModuleRules
{
	public AtomicQueue(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicSystemIncludePaths.Add(ModuleDirectory);
	}
}

