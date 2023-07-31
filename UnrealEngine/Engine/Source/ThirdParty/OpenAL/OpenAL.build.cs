// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;
using System.IO;
using System.Diagnostics;
public class OpenAL : ModuleRules
{
	public OpenAL(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;
		string version = Target.IsInPlatformGroup(UnrealPlatformGroup.Unix) ? "1.18.1" : "1.15.1";

		string OpenALPath = Target.UEThirdPartySourceDirectory + "OpenAL/" + version + "/";
		PublicIncludePaths.Add(OpenALPath + "include");

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			// link against runtime path since this avoids hardcoing an RPATH
			string OpenALRuntimePath = Path.Combine(Target.UEThirdPartyBinariesDirectory , "OpenAL/Linux", Target.Architecture, "libopenal.so");
			PublicAdditionalLibraries.Add(OpenALRuntimePath);

			RuntimeDependencies.Add("$(EngineDir)/Binaries/ThirdParty/OpenAL/Linux/" + Target.Architecture + "/libopenal.so.1");
		}
	}
}
