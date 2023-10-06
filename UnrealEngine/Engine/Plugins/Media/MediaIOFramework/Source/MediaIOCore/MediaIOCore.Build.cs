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
					"ColorManagement",
					"Core",
					"CoreUObject",
					"Engine",
					"ImageWriteQueue",
					"Media",
					"MediaAssets",
					"MediaUtils",
					"MovieSceneCapture",
					"OpenColorIO",
					"Projects",
					"RenderCore",
					"RHI",
					"Slate",
					"SlateCore",
					"TimeManagement"
				});

			PrivateIncludePaths.AddRange(
				new string[] {
				// required for scene view extensions related headers
				Path.Combine(GetModuleDirectory("Renderer"), "Private")
			});


			PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "AudioMixer",
	                "AudioMixerCore",
					"GPUTextureTransfer",
					"ImageWrapper",
					"Renderer",
                    "SignalProcessing", 
	                "SoundFieldRendering"
				});

			PublicDefinitions.Add("WITH_MEDIA_IO_AUDIO_DEBUGGING=0");

			if (Target.bBuildEditor == true)
			{
				PrivateDependencyModuleNames.Add("EditorFramework");
				PrivateDependencyModuleNames.Add("UnrealEd");
				PrivateDependencyModuleNames.Add("LevelEditor");
			}
		}
	}
}
