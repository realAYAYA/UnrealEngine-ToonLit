// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class AndroidFetchBackgroundDownload : ModuleRules
	{
		public AndroidFetchBackgroundDownload(ReadOnlyTargetRules Target) : base(Target)
		{
			ShortName = "AndroidFBGDL";

			PrivateDependencyModuleNames.AddRange(new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"Launch",
					"AndroidBackgroundService",
					"BackgroundHTTP",
				});

			if (Target.Platform == UnrealTargetPlatform.Android)
			{
				string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
				AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(PluginPath, "AndroidFetchBackgroundDownload_UPL.xml"));
			}
		}
	}
}
