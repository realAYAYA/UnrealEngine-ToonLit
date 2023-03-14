// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ADO : ModuleRules
{
	public ADO(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicIncludePaths.Add("$(CommonProgramFiles)");
		}
	}
}

