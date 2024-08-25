// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PakFile : ModuleRules
{
	public PakFile(ReadOnlyTargetRules Target) : base(Target)
	{
		UnsafeTypeCastWarningLevel = WarningLevel.Error;
		PrivateDependencyModuleNames.Add("Core");
		PrivateDependencyModuleNames.Add("CoreUObject");
		PublicDependencyModuleNames.Add("RSA");

		bAllowAutoRTFMInstrumentation = true;
	}
}
