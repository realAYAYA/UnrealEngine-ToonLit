// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Sockets : ModuleRules
{
	public Sockets(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePathModuleNames.Add("NetCommon");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"NetCommon"
			});

		PublicDefinitions.Add("SOCKETS_PACKAGE=1");

		UnsafeTypeCastWarningLevel = WarningLevel.Error;
	}
}
