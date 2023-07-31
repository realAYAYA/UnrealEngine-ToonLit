// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LibOVRAudio : ModuleRules
{
	public LibOVRAudio(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string SourceDirectory = Target.UEThirdPartySourceDirectory + "Oculus/LibOVRAudio/LibOVRAudio/";

		PublicIncludePaths.Add(SourceDirectory + "include");

		// Note: DLL/.so dynamically loaded by FOculusAudioLibraryManager::LoadDll()		
	}
}
