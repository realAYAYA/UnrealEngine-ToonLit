// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CommonConversationEditor : ModuleRules
{
	public CommonConversationEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"CommonConversationRuntime",
				"CommonConversationGraph",
				"AIGraph",
				"UnrealEd",
				"GraphEditor",
				"PropertyEditor",
				"Kismet",
				"InputCore",
				"ApplicationCore",
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
