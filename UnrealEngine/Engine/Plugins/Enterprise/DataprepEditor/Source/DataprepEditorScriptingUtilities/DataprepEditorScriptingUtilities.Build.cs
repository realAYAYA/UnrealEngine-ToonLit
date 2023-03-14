// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class DataprepEditorScriptingUtilities : ModuleRules
	{
		public DataprepEditorScriptingUtilities(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"DataprepCore",
					"Engine",

					// Temporary
					"DataprepEditor",
				}
			);
		}
	}
}