// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CryptoPP : ModuleRules
{
    public CryptoPP(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string LibFolder = "lib/";
		string LibPrefix = "";
        string LibPostfixAndExt = ".";//(Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT) ? "d." : ".";
        string CryptoPPPath = Target.UEThirdPartySourceDirectory + "CryptoPP/5.6.5/";

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PublicSystemIncludePaths.Add(CryptoPPPath + "include");
            PublicSystemIncludePaths.Add(Target.UEThirdPartySourceDirectory);
            LibFolder += "Win64/VS2015/";
            LibPostfixAndExt += "lib";
        }

        PublicAdditionalLibraries.Add(CryptoPPPath + LibFolder + LibPrefix + "cryptlib" + LibPostfixAndExt);
	}

}
