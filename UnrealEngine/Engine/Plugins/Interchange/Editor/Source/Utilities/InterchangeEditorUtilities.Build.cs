// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class InterchangeEditorUtilities : ModuleRules
	{
		public InterchangeEditorUtilities(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"InterchangeCore",
					"InterchangeEngine"
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"DesktopPlatform",
					"Slate",
					"SlateCore",
					"UnrealEd"
				}
			);
		}
	}
}
