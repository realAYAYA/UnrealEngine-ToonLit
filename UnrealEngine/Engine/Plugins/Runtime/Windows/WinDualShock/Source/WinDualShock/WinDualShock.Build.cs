// Copyright Epic Games, Inc. All Rights Reserved.

using System;

namespace UnrealBuildTool.Rules
{
	public class WinDualShock : ModuleRules
	{
		public WinDualShock(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"ApplicationCore",
					"Engine",
					"Slate",
					"InputDevice",
					"AudioMixerCore",
					"AudioMixer",
					"AudioExtensions",
					"Projects"
			});

			// Use reflection to allow type not to exist if console code is not present
			System.Type LibScePadType = System.Type.GetType("LibScePad");
			bool bHasSupport = false;
			string PlatformName = "";
			if (LibScePadType != null)
			{
				bHasSupport = (bool)LibScePadType.GetMethod("GetPadLibLocation").Invoke(null, new object[] { EngineDirectory, Target.WindowsPlatform.Compiler, null });
				PlatformName = (string)LibScePadType.GetMethod("GetPlatformName").Invoke(null, new object[] { EngineDirectory, Target.WindowsPlatform.Compiler });
				if (bHasSupport)
				{
					AddEngineThirdPartyPrivateStaticDependencies(Target,
						"LibScePad",
						"DX11Audio",
						"XAudio2_9"
					);
					string[] Includes = new string[2];
					Includes[0] = "ApplicationCore_Sony";
					Includes[1] = "ApplicationCore_" + PlatformName;
					PrivateIncludePathModuleNames.AddRange(Includes);
				}
			}

			PublicDefinitions.Add("DUALSHOCK4_SUPPORT=" + (bHasSupport ? "1" : "0"));
			PublicDefinitions.Add("DUALSENSE_SUPPORT=" + (PlatformName.Equals("PS5") ? "1" : "0"));
		}
	}
}