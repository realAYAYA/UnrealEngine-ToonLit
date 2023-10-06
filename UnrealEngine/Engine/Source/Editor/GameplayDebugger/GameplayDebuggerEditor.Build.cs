// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class GameplayDebuggerEditor : ModuleRules
	{
		public GameplayDebuggerEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"Engine",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"RenderCore",
					"InputCore",
					"SlateCore",
					"Slate",
					"DeveloperSettings",
				});
			
			if (Target.Configuration != UnrealTargetConfiguration.Shipping)
			{
				PrivateDependencyModuleNames.Add("DrawPrimitiveDebugger");
			}
/*
			PrivateIncludePaths.AddRange(
				new string[] {
					"Developer/Settings/Public",
				});
*/
			if (Target.bBuildEditor)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[] {
						
						"EditorFramework",
						"UnrealEd",
						"LevelEditor",
						"PropertyEditor",
						"GameplayDebugger"
					});
			}

			if (Target.bUseGameplayDebuggerCore)
			{
				PrecompileForTargets = PrecompileTargetsType.Any;
			}
		}
	}
}
