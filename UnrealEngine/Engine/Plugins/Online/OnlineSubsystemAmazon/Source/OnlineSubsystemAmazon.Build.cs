// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class OnlineSubsystemAmazon : ModuleRules
{
	public OnlineSubsystemAmazon(ReadOnlyTargetRules Target) : base(Target)
    {
		PublicDefinitions.Add("ONLINESUBSYSTEMAMAZON_PACKAGE=1");	
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core", 
                "CoreUObject",
				"ApplicationCore",
				"HTTP",
				"Json",
                "OnlineSubsystem", 
			}
		);
	}
}
