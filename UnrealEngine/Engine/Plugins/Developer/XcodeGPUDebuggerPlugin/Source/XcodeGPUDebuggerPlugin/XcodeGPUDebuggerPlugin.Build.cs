// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class XcodeGPUDebuggerPlugin : ModuleRules
	{
		public XcodeGPUDebuggerPlugin(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { });
			PublicDependencyModuleNames.AddRange(new string[]
			{
				  "Core"
				, "CoreUObject"
				, "Engine"
				, "InputCore"
				, "DesktopPlatform"
				, "Projects"
				, "RenderCore"
				, "InputDevice"
				, "RHI"
				, "DeveloperSettings"
			});

			PublicWeakFrameworks.Add("Metal");

			if (Target.bBuildEditor == true)
			{
				DynamicallyLoadedModuleNames.AddRange(new string[] { "LevelEditor" });
				PublicDependencyModuleNames.AddRange(new string[]
				{
					  "Slate"
					, "SlateCore"
					, "UnrealEd"
					, "MainFrame"
					, "GameProjectGeneration"
				});
			}
		}
	}
}
