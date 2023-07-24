// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AudioMixerCore : ModuleRules
	{
		public AudioMixerCore(ReadOnlyTargetRules Target) : base(Target)
		{
            PublicIncludePathModuleNames.Add("SignalProcessing");

			PrivateDependencyModuleNames.AddRange(
				new string[]
                {
                    "Core",
					"SignalProcessing",
					"TraceLog"
                }
			);
		}
	}
}
