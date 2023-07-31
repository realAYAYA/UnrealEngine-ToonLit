// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class JsonUtilities : ModuleRules
{
	public JsonUtilities( ReadOnlyTargetRules Target ) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Json",
			}
		);
	}
}
