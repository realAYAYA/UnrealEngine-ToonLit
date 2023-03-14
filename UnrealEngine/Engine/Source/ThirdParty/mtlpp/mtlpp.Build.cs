// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class MTLPP : ModuleRules
{
	public MTLPP(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string MTLPPPath = Target.UEThirdPartySourceDirectory + "mtlpp/mtlpp-master-7efad47/";

		if (Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.IOS || Target.Platform == UnrealTargetPlatform.TVOS)
		{
			string PlatformName = "";
			if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				PlatformName = "Mac";
			}
			else if (Target.Platform == UnrealTargetPlatform.IOS)
			{
				PlatformName = "IOS";
			}
			else if (Target.Platform == UnrealTargetPlatform.TVOS)
			{
				PlatformName = "TVOS";
			}
		
			PublicIncludePaths.Add(MTLPPPath + "src");
			PublicSystemIncludePaths.Add(MTLPPPath + "interpose");
			
			// A full debug build without any optimisation and validation code enabled
			if (Target.Configuration == UnrealTargetConfiguration.Debug)
			{
				if (Target.bDebugBuildsActuallyUseDebugCRT)
				{
					// Use debug mtlpp library
					PublicAdditionalLibraries.Add(MTLPPPath + "lib/" + PlatformName + "/libmtlppd.a");
				}
				else
				{
					// Use development mtlpp library
					PublicAdditionalLibraries.Add(MTLPPPath + "lib/" + PlatformName + "/libmtlpp.a");
				}
			}
			// A development build that uses mtlpp compiled for release but with validation code enabled 
			else if (Target.Configuration == UnrealTargetConfiguration.Development || Target.Configuration == UnrealTargetConfiguration.DebugGame)
			{
				PublicAdditionalLibraries.Add(MTLPPPath + "lib/" + PlatformName + "/libmtlpp.a");
			}
			// A shipping configuration that disables all validation and is aggressively optimised.
			else
			{
				PublicDefinitions.Add("MTLPP_CONFIG_VALIDATE=0"); // Disables the mtlpp validation used for reporting resource misuse which is compiled out for test/shipping
				PublicAdditionalLibraries.Add(MTLPPPath + "lib/" + PlatformName + "/libmtlpps.a");
			}
		}
    }
}
