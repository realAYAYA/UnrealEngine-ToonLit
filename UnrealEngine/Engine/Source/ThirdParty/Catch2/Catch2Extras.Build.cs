// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;
using UnrealBuildBase;

public class Catch2Extras : ModuleRules
{
	public Catch2Extras(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		// Just setup our include path for catch_amalgamated.hpp/cpp
		PublicSystemIncludePaths.Add(Path.Combine(Unreal.EngineDirectory.FullName, "Source", "ThirdParty", "Catch2", "v3.4.0", "extras"));
	}
}
