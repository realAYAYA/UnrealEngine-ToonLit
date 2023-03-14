// Copyright Epic Games Tools LLC
//   Licenced under the Unreal Engine EULA 

using System.IO;
using System.Collections.Generic;
using UnrealBuildTool;

public class BinkMediaPlayer : ModuleRules 
{
    public BinkMediaPlayer(ReadOnlyTargetRules Target) : base(Target)
    {
		bAllowConfidentialPlatformDefines = true;

        PublicDependencyModuleNames.Add("Core");
        PublicDependencyModuleNames.Add("CoreUObject");
        PublicDependencyModuleNames.Add("Engine");
        PublicDependencyModuleNames.Add("InputCore");
        PublicDependencyModuleNames.Add("RenderCore");
        PublicDependencyModuleNames.Add("RHI");
        PublicDependencyModuleNames.Add("MoviePlayer");
        //PublicDependencyModuleNames.Add("MediaAssets");
        PublicDependencyModuleNames.Add("Projects");

		AddEngineThirdPartyPrivateStaticDependencies(Target, "BinkMediaPlayerSDK");

        if (Target.bBuildEditor == true)
        {
			PublicDependencyModuleNames.Add("Slate");
			PublicDependencyModuleNames.Add("SlateCore");
			PublicDependencyModuleNames.Add("DesktopWidgets");
			PublicDefinitions.Add("BINKPLUGIN_UE4_EDITOR=1");
            PrivateDependencyModuleNames.AddRange(new string[] { "PropertyEditor", "DesktopPlatform", "SourceControl", "EditorStyle", "UnrealEd" });
        }
        else
        {
            PublicDefinitions.Add("BINKPLUGIN_UE4_EDITOR=0");
        }

		//RuntimeDependencies.Add("$(ProjectDir)/Content/Movies/..."); // For chunked streaming

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
			AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(PluginPath, "BinkMediaPlayer_APL.xml"));
		}
	}
}
