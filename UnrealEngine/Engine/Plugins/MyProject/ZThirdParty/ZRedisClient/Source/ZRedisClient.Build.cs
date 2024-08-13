// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ZRedisClient : ModuleRules
{
	public ZRedisClient(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        bEnableUndefinedIdentifierWarnings = true;

        PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
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
				"CoreUObject",
				// "Engine",
				// "Slate",
				// "SlateCore",
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);

        string ThirdPartyPath = "../ThirdParty/";
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, ThirdPartyPath + "hiredis"));
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
	        PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, ThirdPartyPath + "lib/Win64/hiredis.lib"));
        }
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
	        PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, ThirdPartyPath + "lib/Linux/libhiredis.a"));
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
	        PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, ThirdPartyPath + "lib/Mac/libhiredis.a"));
        }
	}
}
