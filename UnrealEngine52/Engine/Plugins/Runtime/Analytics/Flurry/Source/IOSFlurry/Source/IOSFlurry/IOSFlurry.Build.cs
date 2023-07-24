// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class IOSFlurry : ModuleRules
	{
        public IOSFlurry(ReadOnlyTargetRules Target) : base(Target)
		{
			BinariesSubFolder = "NotForLicensees";

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					// ... add other public dependencies that you statically link with here ...
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Analytics",
					// ... add private dependencies that you statically link with here ...
				}
				);

            PublicIncludePathModuleNames.Add("Analytics");

			PublicFrameworks.AddRange(
				new string[] {
					"CoreTelephony",
					"SystemConfiguration",
					"UIKit",
					"Foundation",
					"CoreGraphics",
					"MobileCoreServices",
					"StoreKit",
					"CFNetwork",
					"CoreData",
					"Security",
					"CoreLocation"
				});

			bool bHasFlurrySDK = false; // Temporarily disabled. libFlurry_6.7.0.a was compiled without bitcode enabled, so it cannot be used in Shipping config any more
//                (System.IO.Directory.Exists(System.IO.Path.Combine(Target.UEThirdPartySourceDirectory, "Flurry")) &&
//                  System.IO.Directory.Exists(System.IO.Path.Combine(Target.UEThirdPartySourceDirectory, "Flurry", "IOS"))) ||
//                (System.IO.Directory.Exists(System.IO.Path.Combine(Target.UEThirdPartySourceDirectory, "NotForLicensees")) &&
//                  System.IO.Directory.Exists(System.IO.Path.Combine(Target.UEThirdPartySourceDirectory, "NotForLicensees", "Flurry")) &&
//                  System.IO.Directory.Exists(System.IO.Path.Combine(Target.UEThirdPartySourceDirectory, "NotForLicensees", "Flurry", "IOS")));
            if (bHasFlurrySDK)
            {
                PublicIncludePaths.Add(Target.UEThirdPartySourceDirectory + "NotForLicensees/Flurry/IOS/");
                PublicAdditionalLibraries.Add(Target.UEThirdPartySourceDirectory + "NotForLicensees/Flurry/IOS/libFlurry_6.7.0.a");

                PublicDefinitions.Add("WITH_FLURRY=1");
            }
            else
            {
                PublicDefinitions.Add("WITH_FLURRY=0");
            }
        }
	}
}
