// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class NetCore : ModuleRules
{
	public NetCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange
		(
			new string[]
			{
				"Core",
				"CoreUObject",
				"TraceLog",
				"NetCommon"
			}
		);

		UnsafeTypeCastWarningLevel = WarningLevel.Error;

		bAllowAutoRTFMInstrumentation = true;
	}
}
