// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealHeaderTool : ModuleRules
{
	public UnrealHeaderTool(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.Add("Runtime/Launch/Public");

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Json",
				"Projects"
			}
		);
	
		PrivateIncludePaths.AddRange(
			new string[]
			{
				// For LaunchEngineLoop.cpp includes
				"Runtime/Launch/Private",
				"Runtime/RHI/Public",
				"Programs/UnrealHeaderTool/Private",
				// For RigVM Defines
				"Runtime/RigVM/Public",
			});
		
		bEnableExceptions = true;

		UnsafeTypeCastWarningLevel = WarningLevel.Error;
	}
}
