// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class DataInterfaceGraphEditor : ModuleRules
	{
		public DataInterfaceGraphEditor(ReadOnlyTargetRules Target) : base(Target)
		{

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"AssetTools",
					"DataInterface",
					"DataInterfaceGraph",
					"DataInterfaceGraphUncookedOnly",
					"UnrealEd",
					"SlateCore",
					"Slate",
					"PropertyEditor",
					"RigVM",
					"RigVMDeveloper",
					"ControlRig",
					"ControlRigDeveloper",
					"ControlRigEditor",
					"GraphEditor",
				}
			);
		}
	}
}