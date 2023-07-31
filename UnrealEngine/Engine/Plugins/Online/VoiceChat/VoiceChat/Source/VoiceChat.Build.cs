// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class VoiceChat : ModuleRules
	{
		public VoiceChat(ReadOnlyTargetRules Target) : base(Target)
		{
			// External so we don't build a DLL for this, it is header only.
			Type = ModuleType.External;

			PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core"
				}
			);
		}
	}
}
