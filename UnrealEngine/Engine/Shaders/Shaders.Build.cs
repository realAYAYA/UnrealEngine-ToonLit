// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class Shaders : ModuleRules
	{
		public Shaders(ReadOnlyTargetRules Target) : base(Target)
		{
			// External so we don't build a DLL for this, it is header only.
			Type = ModuleType.External;
			ModuleIncludePathWarningLevel = WarningLevel.Off;

			PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));
			PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Shared"));
		}
	}
}
