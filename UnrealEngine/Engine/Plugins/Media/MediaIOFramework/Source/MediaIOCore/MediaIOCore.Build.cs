// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class MediaIOCore : ModuleRules
	{
		public MediaIOCore(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"AudioExtensions",
					"Core",
					"CoreUObject",
					"Engine",
					"ImageWriteQueue",
					"Media",
					"MediaAssets",
					"MediaUtils",
					"MovieSceneCapture",
					"Projects",
					"RenderCore",
					"RHI",
					"Slate",
					"SlateCore",
					"TimeManagement"
				});

			var EngineDir = Path.GetFullPath(Target.RelativeEnginePath);
			PrivateIncludePaths.AddRange(
				new string[] {
					//required for FScreenPassVS and AddDrawScreenPass
					Path.Combine(EngineDir, "Source", "Runtime", "Renderer", "Private")
				});

			PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "AudioMixer",
	                "AudioMixerCore",
					"GPUTextureTransfer",
					"ImageWrapper",
                    "RenderCore",
					"Renderer",
                    "SignalProcessing", 
	                "SoundFieldRendering"
				});

			if (Target.bBuildEditor == true)
			{
				PrivateDependencyModuleNames.Add("EditorFramework");
				PrivateDependencyModuleNames.Add("UnrealEd");
				PrivateDependencyModuleNames.Add("LevelEditor");
			}
		}
	}
}
