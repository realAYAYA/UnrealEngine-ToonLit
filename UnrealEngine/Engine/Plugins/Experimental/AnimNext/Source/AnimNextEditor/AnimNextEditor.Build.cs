// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AnimNextEditor : ModuleRules
	{
		public AnimNextEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"AssetTools",
					"UnrealEd",
					"AnimNext",
					"AnimNextUncookedOnly",
					"UnrealEd",
					"SlateCore",
					"Slate",
					"InputCore",
					"PropertyEditor",
					"RigVM",
					"RigVMDeveloper",
					"ControlRig",
					"ControlRigDeveloper",
					"ControlRigEditor",
					"GraphEditor",
					"ToolWidgets",
					"ToolMenus",
					"AssetDefinition",
					"SourceControl", 
					"KismetWidgets",
					"StructUtilsEditor",
					"BlueprintGraph",	// For K2 Schema
					"DesktopWidgets",
					"ContentBrowserFileDataSource",
					"SubobjectEditor",
				}
			);
		}
	}
}