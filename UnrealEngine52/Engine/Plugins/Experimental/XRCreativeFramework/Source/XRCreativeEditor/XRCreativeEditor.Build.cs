// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class XRCreativeEditor : ModuleRules
{
	public XRCreativeEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
			}
		);

		PrivateDependencyModuleNames.AddRange(new string[]
			{
				"OpenXRHMD",
				"OpenXRInput",
				"UnrealEd",
				"VREditor",
				"EnhancedInput",
				"HeadMountedDisplay",
				"InputCore",
				"InputEditor",
				"InteractiveToolsFramework",
				"LevelEditor",
				"RenderCore",
				"Slate",
				"SlateCore",
				"UMG",
				"XRCreative",
			}
		);
	}
}
