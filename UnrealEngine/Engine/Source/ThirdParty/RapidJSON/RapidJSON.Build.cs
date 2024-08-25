// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class RapidJSON : ModuleRules
{
	public RapidJSON(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicSystemIncludePaths.Add(Path.Combine(Target.UEThirdPartySourceDirectory, "RapidJSON", "1.1.0"));
	}
}
