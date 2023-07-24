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

			if (Target.bUseGameplayDebuggerCore)
			{
				PrecompileForTargets = PrecompileTargetsType.Any;
			}

			if (Target.bUseGameplayDebugger || Target.bUseGameplayDebuggerCore)
			{
				PublicDefinitions.Add("WITH_GAMEPLAY_DEBUGGER_CORE=1");
				PublicDefinitions.Add("WITH_GAMEPLAY_DEBUGGER=" + (Target.bUseGameplayDebugger ? 1 : 0));
				if (Target.bUseGameplayDebugger || (Target.bUseGameplayDebuggerCore && Target.Configuration != UnrealTargetConfiguration.Shipping))
				{
					PublicDefinitions.Add("WITH_GAMEPLAY_DEBUGGER_MENU=1");
				}
				else
				{
					PublicDefinitions.Add("WITH_GAMEPLAY_DEBUGGER_MENU=0");
				}
			}
			else
			{
				PublicDefinitions.Add("WITH_GAMEPLAY_DEBUGGER_CORE=0");
				PublicDefinitions.Add("WITH_GAMEPLAY_DEBUGGER=0");
				PublicDefinitions.Add("WITH_GAMEPLAY_DEBUGGER_MENU=0");
			}
		}
	}
}
