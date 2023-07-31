// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;

public class RHICore : ModuleRules
{
	public RHICore(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(new string[] { "RHI" });
		PublicDependencyModuleNames.AddRange(new string[] { "RenderCore" });
		PrivateDependencyModuleNames.AddRange(new string[] { "Core", "Projects", "RHI", "ApplicationCore", "TraceLog" });
	}
}
