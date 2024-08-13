// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class ZProtobuf : ModuleRules
{
	public ZProtobuf(ReadOnlyTargetRules Target) : base(Target)
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

		PublicIncludePaths.Add(ModuleDirectory);
		PrivateIncludePaths.Add(ModuleDirectory);

		if (Target.bForceEnableRTTI)
		{
			bUseRTTI = true;
			PublicDefinitions.Add("GOOGLE_PROTOBUF_NO_RTTI=0");
		}
		else
		{
			bUseRTTI = false;
			PublicDefinitions.Add("GOOGLE_PROTOBUF_NO_RTTI=1");
		}
		if (Target.Platform != UnrealTargetPlatform.Win64)
		{
			PublicDefinitions.Add("HAVE_PTHREAD");
		}

		if (Target.Platform != UnrealTargetPlatform.IOS && Target.Platform != UnrealTargetPlatform.Mac)
		{
			PublicDefinitions.Add("PROTOBUF_USE_DLLS");
		}

		ShadowVariableWarningLevel = WarningLevel.Off;
		bEnableUndefinedIdentifierWarnings = false;
		bEnableExceptions = true;

	}
}
