// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class ZNatsClient : ModuleRules
{
	public ZNatsClient(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

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
				// "CoreUObject",
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

		// ------

        string ThirdPartyPath = "../ThirdParty/";
        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, ThirdPartyPath + "include"));
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
	        PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, ThirdPartyPath + "lib/Win64/nats_static.lib"));
        }
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
	        PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, ThirdPartyPath + "lib/Linux/libnats_static.a"));
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
	        PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, ThirdPartyPath + "lib/Mac/libnats_static.a"));
        }

	}
}
