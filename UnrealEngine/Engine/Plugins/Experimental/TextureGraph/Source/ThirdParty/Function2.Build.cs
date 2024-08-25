// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;
using System.IO;

public class Function2 : ModuleRules
{
	public Function2(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string incDir = Path.Combine(ModuleDirectory, "inc");
		PublicSystemIncludePaths.Add(Path.Combine(incDir, "function2"));
	}
}
