// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class GameplayAbilities : ModuleRules
	{
		public GameplayAbilities(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.Add("GameplayAbilities/Private");
			
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"NetCore",
					"Engine",
					"GameplayTags",
					"GameplayTasks",
					"MovieScene",
					"PhysicsCore",
					"DataRegistry"
				}
				);

			// Niagara support for gameplay cue notifies.
			{
				PrivateDependencyModuleNames.Add("Niagara");
			}

			if (Target.bBuildEditor == true)
			{
				PrivateDependencyModuleNames.Add("EditorFramework");
				PrivateDependencyModuleNames.Add("UnrealEd");
				PrivateDependencyModuleNames.Add("Slate");
				PrivateDependencyModuleNames.Add("SequenceRecorder");
			}

			SetupGameplayDebuggerSupport(Target);

			SetupIrisSupport(Target);
		}
	}
}
