// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Bolts : ModuleRules
{
	public Bolts(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;

        PublicDefinitions.Add("WITH_BOLTS=1");

		string BoltsPath = Target.UEThirdPartySourceDirectory + "IOS/Bolts/";
        if (Target.Platform == UnrealTargetPlatform.IOS)
        {
            BoltsPath += "Bolts/";

			PublicSystemIncludePaths.Add(BoltsPath + "Bolts/Include");

			string LibDir = BoltsPath + "Lib/Release/";

            PublicAdditionalLibraries.Add(LibDir + "Bolts");
        }
    }
}
