// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class Facebook : ModuleRules
{
	public Facebook(ReadOnlyTargetRules Target) : base(Target)
    {
		Type = ModuleType.External;

		// Additional Frameworks and Libraries for Android found in OnlineSubsystemFacebook_UPL.xml
        if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			PublicDefinitions.Add("WITH_FACEBOOK=1");

            // These are iOS system libraries that Facebook depends on
            //PublicFrameworks.AddRange(
            //new string[] {
            //    "ImageIO"
            //});

            // More dependencies for Facebook
            //PublicAdditionalLibraries.AddRange(
            //new string[] {
            //    "xml2"
            //});



			// Access to Facebook core
			PublicAdditionalFrameworks.Add(
				new Framework(
					"FBSDKCoreKit",
					"IOS/FacebookSDK/FBSDKCoreKit.embeddedframework.zip"
				)
			);

			// Access to Facebook login
			PublicAdditionalFrameworks.Add(
				new Framework(
					"FBSDKLoginKit",
					"IOS/FacebookSDK/FBSDKLoginKit.embeddedframework.zip"
				)
			);


			// commenting out over if(false) for #jira FORT-77943 per Peter.Sauerbrei prior change with CL 3960071
			//// Access to Facebook places
			//PublicAdditionalFrameworks.Add(
			//	new UEBuildFramework(
			//		"FBSDKPlacesKit",
			//		"IOS/FacebookSDK/FBSDKPlacesKit.embeddedframework.zip"
			//	)
			//);

			// Access to Facebook sharing
			PublicAdditionalFrameworks.Add(
				new Framework(
					"FBSDKShareKit",
					"IOS/FacebookSDK/FBSDKShareKit.embeddedframework.zip"
				)
			);
		}
	}
}

