// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class WebBrowserNativeProxy : ModuleRules
    {
        public WebBrowserNativeProxy(ReadOnlyTargetRules Target) : base(Target)
        {
            PublicDependencyModuleNames.AddRange(
                new string[]
                {
                    "Core",
                    "CoreUObject",
                    "WebBrowser"
                }
            );

            PrivateDependencyModuleNames.AddRange(
            new string[]
                {
                    "SlateCore",
                    "Slate",
                }
            );
        }
    }
}