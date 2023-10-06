// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class EventLoopUnitTests : TestModuleRules
{
	public EventLoopUnitTests(ReadOnlyTargetRules Target) : base(Target)
	{
		OptimizeCode = CodeOptimization.Never;

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"EventLoop",
				"Sockets"
			});

		UpdateBuildGraphPropertiesFile(new Metadata() { TestName = "EventLoop", TestShortName = "EventLoop" });
	}
}