// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealInsights : ModuleRules
{
	public UnrealInsights(ReadOnlyTargetRules Target) : base(Target)
	{
		UnsafeTypeCastWarningLevel = WarningLevel.Error;

		PublicIncludePathModuleNames.Add("Launch");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"AppFramework",
				"ApplicationCore",
				"Core",
				"CoreUObject",
				"Projects",
				"Slate",
				"SlateCore",
				"SourceCodeAccess",
				"StandaloneRenderer",
				"TargetPlatform",
				"TraceInsights",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"SlateReflector"
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"SlateReflector"
			}
		);

		if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PrivateDependencyModuleNames.Add("XCodeSourceCodeAccess");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "CEF3");
		}
		else if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PrivateDependencyModuleNames.Add("VisualStudioSourceCodeAccess");
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.IOS))
		{
			PrivateDependencyModuleNames.AddRange(
				new string [] {
					"NetworkFile",
					"StreamingFile"
				}
			);
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"UnixCommonStartup"
				}
			);
		}
	}
}
