// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class KismetCompiler : ModuleRules
{
	public KismetCompiler(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"FieldNotification",
				"InputCore",
				"EditorFramework",
				"UnrealEd",
				"MovieScene",
				//"MovieSceneTools",
				"BlueprintGraph",
				"AnimGraph",
				"MessageLog",
				"Kismet",
				"ScriptDisassembler",
			}
			);
	}
}
