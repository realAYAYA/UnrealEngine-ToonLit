// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class InstanceDataObjectFixupTool : ModuleRules
	{
		public InstanceDataObjectFixupTool(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"Engine",
					"CoreUObject",
					"StructUtils",
				}
			);
			
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"SlateCore",
					"Slate",
					"PropertyEditor", 
					"WorkspaceMenuStructure", 
					"UnrealEd",
				}
			);
		}
	}
}