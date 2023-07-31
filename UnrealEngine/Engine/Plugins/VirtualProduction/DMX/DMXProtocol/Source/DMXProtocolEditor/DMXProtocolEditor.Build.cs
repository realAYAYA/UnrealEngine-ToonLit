// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DMXProtocolEditor : ModuleRules
{
	public DMXProtocolEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"DMXProtocol",
				"Networking",
				"Sockets",
				"PropertyEditor"
			}
		);
				
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				
				"Engine",
				"InputCore",
				"Projects",
				"Slate",
				"SlateCore",
				"TimeManagement",
				"UnrealEd"
			}
		);

	}
}
