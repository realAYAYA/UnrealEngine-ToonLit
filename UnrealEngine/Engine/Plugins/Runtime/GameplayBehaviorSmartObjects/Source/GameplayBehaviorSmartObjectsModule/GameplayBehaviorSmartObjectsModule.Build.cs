// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GameplayBehaviorSmartObjectsModule : ModuleRules
{
	public GameplayBehaviorSmartObjectsModule(ReadOnlyTargetRules Target) : base(Target)
	{
		ShortName = "GameplayBhvSO";
		
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"AIModule",
				"Core",
				"GameplayBehaviorsModule",
				"GameplayTags",
				"GameplayTasks",
				"SmartObjectsModule"
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"MassEntity"
			}
			);
	}
}
