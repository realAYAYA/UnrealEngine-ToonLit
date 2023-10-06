// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class XRBaseEditor : ModuleRules
	{
		public XRBaseEditor(ReadOnlyTargetRules Target) : base(Target)
		{
            PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"PropertyEditor",
					"InputCore",
					"SlateCore",
					"Slate",
					"HeadMountedDisplay",
					"XRBase",
				}
			);
			
			if (Target.bBuildEditor == true)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
					});
			}
        }
	}
}
