// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RemoteControlComponents : ModuleRules
{
	public RemoteControlComponents(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"RemoteControl"
			}
			);
	

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Engine",
			}
			);


		if (Target.Type == TargetRules.TargetType.Editor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"RemoteControlUI",
				"UnrealEd",
			});
		}
	}
}
