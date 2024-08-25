// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class ToolMenusTests : TestModuleRules
{
	protected Metadata ToolMenusTestsMetadata = new Metadata() {
		TestName = "ToolMenus",
		TestShortName = "ToolMenus",
		ReportType = "xml",
		SupportedPlatforms = {
			UnrealTargetPlatform.Win64,
			UnrealTargetPlatform.Linux,
			UnrealTargetPlatform.Mac } };

	/// <summary>
	/// Test metadata to be used with BuildGraph
	/// </summary>
	public Metadata TestMetadata
	{ 
		get { return ToolMenusTestsMetadata; }
	}

	public ToolMenusTests(ReadOnlyTargetRules Target) : base(Target, true)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Slate",
				"SlateCore",
				"ToolMenus"
			});

		if (Target.bBuildWithEditorOnlyData)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"DesktopPlatform"
				});
		}

		string PlatformCompilationArgs;
		foreach (var Platform in UnrealTargetPlatform.GetValidPlatforms())
		{
			if (Platform == UnrealTargetPlatform.Android)
			{
				PlatformCompilationArgs = "-allmodules -architectures=arm64";
			}
			else
			{
				PlatformCompilationArgs = "-allmodules";
			}
			TestMetadata.PlatformCompilationExtraArgs.Add(Platform, PlatformCompilationArgs);
		}
		UpdateBuildGraphPropertiesFile(TestMetadata);
	}
}
