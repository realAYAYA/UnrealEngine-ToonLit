// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class DrawPrimitiveDebugger : ModuleRules
	{
		public DrawPrimitiveDebugger(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"Engine"
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"RenderCore",
					"Renderer",
					"InputCore",
					"SlateCore",
					"Slate",
					"DeveloperSettings",
				});

			PrivateIncludePathModuleNames.Add("Settings");

			// TODO: Decide whether Test builds should also remain restricted
			if (Target.bBuildDeveloperTools || (Target.Configuration != UnrealTargetConfiguration.Shipping))
			{
				PrecompileForTargets = PrecompileTargetsType.Any;
			}
		}
	}
}