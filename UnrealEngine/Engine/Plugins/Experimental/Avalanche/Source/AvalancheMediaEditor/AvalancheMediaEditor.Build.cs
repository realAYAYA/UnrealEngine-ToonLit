// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AvalancheMediaEditor : ModuleRules
{
	public AvalancheMediaEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"AvalancheCore",
				"AvalancheMedia",
				"Core",
				"CoreUObject",
				"DeveloperSettings",
				"Engine",
				"MediaIOCore",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"ApplicationCore",
				"AssetDefinition",
				"AssetTools",
				"Avalanche",
				"AvalancheEditorCore",
				"AvalancheRemoteControl",
				"AvalancheTag",
				"ContentBrowser",
				"EditorWidgets",
				"GraphEditor",
				"ImageCore",
				"InputCore",
				"Json",
				"JsonSerialization",
				"JsonUtilities",
				"Kismet",
				"MediaIOEditor",
				"Messaging",
				"Projects",
				"PropertyEditor",
				"RHI",
				"RemoteControl",
				"RemoteControlLogic",
				"RemoteControlUI", 
				"RenderCore",
				"Serialization",
				"Slate",
				"SlateCore",
				"ToolMenus",
				"UnrealEd",
				"XmlSerialization",
			}
		);
	}
}
