// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AnimNextInterfaceGraphEditor : ModuleRules
	{
		public AnimNextInterfaceGraphEditor(ReadOnlyTargetRules Target) : base(Target)
		{

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"AssetTools",
					"AnimNextInterface",
					"AnimNextInterfaceGraph",
					"AnimNextInterfaceGraphUncookedOnly",
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