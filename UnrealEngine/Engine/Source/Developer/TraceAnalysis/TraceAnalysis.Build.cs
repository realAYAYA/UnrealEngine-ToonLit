// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TraceAnalysis : ModuleRules
{
	public TraceAnalysis(ReadOnlyTargetRules Target) : base(Target)
	{
		// Asio does not compile with C++20, remove if updated
		CppStandard = CppStandardVersion.Cpp17;

		PrivateDependencyModuleNames.Add("Asio");
		PrivateDependencyModuleNames.Add("Cbor");
		PrivateDependencyModuleNames.Add("Core");
		PrivateDependencyModuleNames.Add("Sockets");
		PrivateDependencyModuleNames.Add("TraceLog");
	}
}
