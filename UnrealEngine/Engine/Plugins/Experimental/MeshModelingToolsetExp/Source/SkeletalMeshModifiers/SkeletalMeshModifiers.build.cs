// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SkeletalMeshModifiers : ModuleRules
{
	public SkeletalMeshModifiers(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
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
				"MeshDescription",
				"SkeletalMeshDescription",
				"SkeletalMeshUtilitiesCommon",
				"RenderCore",
				"CoreUObject",
				"Engine",
				"ApplicationCore"
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
