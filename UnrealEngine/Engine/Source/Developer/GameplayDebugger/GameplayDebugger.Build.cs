// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class GameplayDebugger : ModuleRules
	{
		public GameplayDebugger(ReadOnlyTargetRules Target) : base(Target)
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

			PrivateIncludePaths.AddRange(
				new string[] {
					"Developer/Settings/Public",
				});

			if (Target.bBuildEditor)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[] {
						
						"EditorFramework",
						"UnrealEd",
						"LevelEditor",
						"PropertyEditor",
					});
			}

			SetupIrisSupport(Target);

			if (Target.bUseGameplayDebugger)
			{
				PrecompileForTargets = PrecompileTargetsType.Any;
			}
		}
	}
}
