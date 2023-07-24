// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TraceAnalysis : ModuleRules
{
	public TraceAnalysis(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.Add("Asio");
		PrivateDependencyModuleNames.Add("Cbor");
		PrivateDependencyModuleNames.Add("Core");
		PrivateDependencyModuleNames.Add("Sockets");
		PrivateDependencyModuleNames.Add("TraceLog");
	}
}
