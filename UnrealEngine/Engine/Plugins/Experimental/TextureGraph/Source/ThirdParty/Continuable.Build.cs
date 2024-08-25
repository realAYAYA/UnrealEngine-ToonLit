// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;
using System.IO;

public class Continuable : ModuleRules
{
	public Continuable(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string incDir = Path.Combine(ModuleDirectory, "inc");
		PublicSystemIncludePaths.Add(Path.Combine(incDir, "continuable"));
	}
}
