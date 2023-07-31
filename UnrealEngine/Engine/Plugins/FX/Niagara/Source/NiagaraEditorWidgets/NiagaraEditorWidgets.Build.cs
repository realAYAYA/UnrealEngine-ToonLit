// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class NiagaraEditorWidgets : ModuleRules
{
	public NiagaraEditorWidgets(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.AddRange(new string[] {
			System.IO.Path.Combine(GetModuleDirectory("Niagara"), "Private"),
			System.IO.Path.Combine(GetModuleDirectory("NiagaraEditor"), "Private"),
			System.IO.Path.Combine(GetModuleDirectory("GraphEditor"), "Private"),
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
            "NiagaraCore",
			"Niagara",
			"NiagaraEditor",
			"Engine",
			"Core",
			"CoreUObject",
			"Slate",
			"SlateCore",
			"InputCore",
			"EditorFramework",
			"UnrealEd",
			"GraphEditor",
			"EditorStyle",
			"PropertyEditor",
			"AppFramework",
			"Projects",
			"Sequencer",
            "EditorWidgets",
			"ApplicationCore",
			"CurveEditor",
			"DesktopPlatform",
			"ToolWidgets",
			"KismetWidgets"
		});

		PrivateIncludePathModuleNames.AddRange(new string[] {
		});

		PublicDependencyModuleNames.AddRange(new string[] {
		});

		PublicIncludePathModuleNames.AddRange(new string[] {
		});
	}
}
