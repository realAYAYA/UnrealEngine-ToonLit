// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;
using System.Collections.Generic;

public class Vcpkg : ModuleRules
{
	public Vcpkg(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		// This is an empty External module that contains helper scripts for building third party libraries
		// using vcpkg and should not be reference directly.

		// TODO: Support building vcpkg libraries automatically during a build.
	}
}
