// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WebTests : TestModuleRules
{
	public WebTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"ApplicationCore",
				"Core",
				"HTTP",
				"HTTPServer",
				"WebSockets",
				"Json"
			});
		
		UpdateBuildGraphPropertiesFile(new Metadata() { 
			TestName = "Web", 
			TestShortName = "Web", 
			ReportType = "xml", 
		});
	}
}


