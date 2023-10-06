// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;
using System.IO;

public class SDL2 : ModuleRules
{
	protected virtual string SDL2Version { get { return "SDL-gui-backend"; } }
	protected virtual string IncRootDirectory { get { return Target.UEThirdPartySourceDirectory; } }
	protected virtual string LibRootDirectory { get { return Target.UEThirdPartySourceDirectory; } }

	protected virtual string SDL2IncPath { get { return Path.Combine(IncRootDirectory, "SDL2", SDL2Version, "include"); } }
	protected virtual string SDL2LibPath { get { return Path.Combine(LibRootDirectory, "SDL2", SDL2Version, "lib"); } }

	public SDL2(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		// assume SDL to be built with extensions
		PublicDefinitions.Add("SDL_WITH_EPIC_EXTENSIONS=1");

		PublicSystemIncludePaths.Add(SDL2IncPath);

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			if (Target.Configuration == UnrealTargetConfiguration.Debug)
			{
				// Debug version should be built with -fPIC and usable in all targets
				PublicAdditionalLibraries.Add(Path.Combine(SDL2LibPath, "Unix", Target.Architecture.LinuxName, "libSDL2_fPIC_Debug.a"));
			}
			else
			{
				PublicAdditionalLibraries.Add(Path.Combine(SDL2LibPath, "Unix", Target.Architecture.LinuxName, "libSDL2_fPIC.a"));
			}
		}
	}
}
