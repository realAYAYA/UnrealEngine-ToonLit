// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

public class TraceLog : ModuleRules
{
	public TraceLog(ReadOnlyTargetRules Target) : base(Target)
	{
		UnsafeTypeCastWarningLevel = WarningLevel.Error;

		bRequiresImplementModule = false;
		PublicIncludePathModuleNames.Add("Core");
	}
}
