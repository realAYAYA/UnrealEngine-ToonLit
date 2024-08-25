// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class USDStageEditorViewModels : ModuleRules
	{
		public USDStageEditorViewModels(ReadOnlyTargetRules Target) : base(Target)
		{
			// Does not compile with C++20:
			// error C4002: too many arguments for function-like macro invocation 'TF_PP_CAT_IMPL'
			// warning C5103: pasting '"TF_LOG_STACK_TRACE_ON_ERROR"' and '"TF_LOG_STACK_TRACE_ON_WARNING"' does not result in a valid preprocessing token
			CppStandard = CppStandardVersion.Cpp17;

			// Replace with PCHUsageMode.UseExplicitOrSharedPCHs when this plugin can compile with cpp20
			PCHUsage = PCHUsageMode.NoPCHs;

			bUseRTTI = true;

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Boost",
					"Core",
					"CoreUObject",
					"DesktopWidgets",
					"EditorFramework",
					"Engine",
					"InputCore",
					"LevelEditor",
					"Slate",
					"SlateCore",
					"UnrealEd",
					"UnrealUSDWrapper",
					"USDClasses",
					"USDStage",
					"USDStageImporter",
					"USDUtilities",
					"WorkspaceMenuStructure",
				}
			);
		}
	}
}
