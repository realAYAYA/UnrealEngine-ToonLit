// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class WebBrowser : ModuleRules
{
	public WebBrowser(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"ApplicationCore",
				"RHI",
				"InputCore",
				"Serialization",
				"HTTP"
			}
		);

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
				"Slate",
				"SlateCore"
            }
        );

        if (Target.Platform == UnrealTargetPlatform.Android ||
		    Target.Platform == UnrealTargetPlatform.IOS ||
		    Target.Platform == UnrealTargetPlatform.TVOS)
		{
			// We need these on mobile for external texture support
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Engine",
					"Launch",
					"WebBrowserTexture"
				}
			);

			// We need this one on Android for URL decoding
			PrivateDependencyModuleNames.Add("HTTP");

			CircularlyReferencedDependentModules.Add("WebBrowserTexture");
		}

		if ( Target.Type != TargetType.Program && Target.Platform == UnrealTargetPlatform.Win64)
        {
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Engine",
					"RenderCore"
				}
			);

		}

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PublicSystemLibraries.Add("libjnigraphics");
		}

		if (Target.Platform == UnrealTargetPlatform.Win64
		||  Target.Platform == UnrealTargetPlatform.Mac
		||  Target.Platform == UnrealTargetPlatform.Linux)
		{
			PrivateDependencyModuleNames.Add("CEF3Utils");
			AddEngineThirdPartyPrivateStaticDependencies(Target,
				"CEF3"
				);

			if (Target.Type != TargetType.Server)
			{
				if (Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.Linux)
				{
					RuntimeDependencies.Add("$(EngineDir)/Binaries/" + Target.Platform.ToString() + "/EpicWebHelper");
				}
				else
				{
					RuntimeDependencies.Add("$(EngineDir)/Binaries/" + Target.Platform.ToString() + "/EpicWebHelper.exe");
				}
			}
		}
		PrivateDefinitions.Add("PLATFORM_SPECIFIC_WEB_BROWSER=" + (bPlatformSpecificWebBrowser ? "1" : "0"));
	}
	protected virtual bool bPlatformSpecificWebBrowser { get { return false; } }
}
