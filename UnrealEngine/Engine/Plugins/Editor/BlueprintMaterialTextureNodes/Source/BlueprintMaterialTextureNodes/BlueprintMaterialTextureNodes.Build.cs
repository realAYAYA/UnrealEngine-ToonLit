// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class BlueprintMaterialTextureNodes : ModuleRules
{
	public BlueprintMaterialTextureNodes(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
                "Runtime/Engine/Classes/Materials",
				
				// ... add public include paths required here ...
			}
			);			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"ImageCore",
				"AssetRegistry",
                "AssetTools",
                "RHI",
                "StaticMeshEditor",
				
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
                "CoreUObject",
				"EditorFramework",
                "UnrealEd",
                "Engine",
                "PropertyEditor",
                "Slate",
                "SlateCore",

				// ... add private dependencies that you statically link with here ...	
			}
			);

        PrivateIncludePathModuleNames.AddRange(
           new string[]
           {
                "Settings",
                "AssetTools",
                "AssetRegistry",
               //"StaticMeshEditor",
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
