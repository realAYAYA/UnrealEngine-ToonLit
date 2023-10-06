// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AudioCodecEngine : ModuleRules
	{
		public AudioCodecEngine(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
                    "Engine",
					"AudioExtensions",
				//	"TargetPlatform"
				}
			);
		}
	}
}
