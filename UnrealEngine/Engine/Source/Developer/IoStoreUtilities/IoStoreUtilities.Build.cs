// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class IoStoreUtilities : ModuleRules
{
	public IoStoreUtilities (ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateIncludePathModuleNames.AddRange(new string[] {
			"TargetPlatform",
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
			"AssetRegistry",
			"CookMetadata",
			"Projects",
			"Zen",
			"RenderCore",
			"Sockets",
		});

		PublicIncludePathModuleNames.AddRange(new string[] {
			"Zen",
		});

		PrivateDependencyModuleNames.Add("PakFile");
        PrivateDependencyModuleNames.Add("Json");
        PrivateDependencyModuleNames.Add("RSA");
        PrivateDependencyModuleNames.Add("DeveloperToolSettings");
        PrivateDependencyModuleNames.Add("SandboxFile");
        PrivateDependencyModuleNames.Add("IoStoreOnDemand");
	}
}
