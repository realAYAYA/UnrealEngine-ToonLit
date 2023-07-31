// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AndroidLocalNotification : ModuleRules
{
	public AndroidLocalNotification(ReadOnlyTargetRules Target) : base(Target)
	{
		BinariesSubFolder = "Android";

        PublicIncludePaths.AddRange(new string[]
        {
            "Runtime/Android/AndroidLocalNotification/Public",
            "Runtime/Engine/Public",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
            "Launch"
		});
	}
}
