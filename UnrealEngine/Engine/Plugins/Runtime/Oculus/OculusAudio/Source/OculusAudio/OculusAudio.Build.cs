// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class OculusAudio : ModuleRules
	{
		public OculusAudio(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePathModuleNames.AddRange(
				new string[]
				{
					"TargetPlatform"
				}
				);

			PrivateIncludePaths.AddRange(
				new string[] {
					// Relative to Engine\Plugins\Runtime\Oculus\OculusAudio\Source
					"OculusAudio/Private",
					System.IO.Path.Combine(GetModuleDirectory("AudioMixer"), "Private"),

					// ... add other private include paths required here ...
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"AudioMixer",
					"Landscape",
					"SoundFieldRendering",
					"LibOVRAudio",
					"SignalProcessing",
					"AudioExtensions"
				}
				);

			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				// Automatically copy DLL to packaged builds
				RuntimeDependencies.Add("$(EngineDir)/Binaries/ThirdParty/Oculus/Audio/Win64/ovraudio64.dll");

				AddEngineThirdPartyPrivateStaticDependencies(Target, "DX11Audio");
			}

			if (Target.Platform == UnrealTargetPlatform.Android)
			{
				// AndroidPlugin
				{
					string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
					AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(PluginPath, "OculusAudio_APL.xml"));

                    PublicAdditionalLibraries.Add("ThirdParty/Oculus/LibOVRAudio/LibOVRAudio/lib/arm64-v8a/libovraudio64.so");
                }
			}
		}
	}
}
