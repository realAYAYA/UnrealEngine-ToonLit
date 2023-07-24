// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
namespace UnrealBuildTool.Rules
{
	public class AvfMediaCapture : ModuleRules
	{
		public AvfMediaCapture(ReadOnlyTargetRules Target) : base(Target)
		{
			DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"Media",
			});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"Engine",
					"ApplicationCore",
					"MediaUtils",
					"RenderCore",
					"RHI"
				});
			
			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Media",
				});
			
			var EngineDir = Path.GetFullPath(Target.RelativeEnginePath);
			PublicIncludePaths.AddRange(
				new string[] {
					Path.Combine(EngineDir, "Plugins/Media/AvfMedia/Source/AvfMedia/Public/")
				});
			
			PublicFrameworks.AddRange(
				new string[] {
					"CoreMedia",
					"CoreVideo",
					"AVFoundation",
					"AudioToolbox",
					"MediaToolbox",
					"QuartzCore"
				});
		}
	}
}
