// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ChooserEditor : ModuleRules
	{
		public ChooserEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"AssetTools",
					"DataInterface",
					"DataInterfaceGraphEditor", // for DataInterfaceClassFilter, and Widget Factory system (could be moved to a more generic DataInterfaceEditor module
					"Chooser",
					"UnrealEd",
					"EditorWidgets",
					"SlateCore",
					"Slate",
					"PropertyEditor",
					"RigVM",
					"RigVMDeveloper",
					"ControlRig",
					"ControlRigDeveloper",
					"ControlRigEditor",
					"GraphEditor",
					"InputCore",
					"EditorStyle",
					// ... add private dependencies that you statically link with here ...
				}
			);
		}
	}
}