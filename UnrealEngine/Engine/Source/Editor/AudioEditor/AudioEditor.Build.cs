// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AudioEditor : ModuleRules
{
	public AudioEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange
		(
			new string[]
			{
				"AudioMixer",
				"EditorSubsystem",
				"GameProjectGeneration",
				"ToolMenus",
				"UMG",
				"DeveloperSettings",
				"UMGEditor",
				"AudioExtensions",
				"AudioLinkEngine"
			}
		);

		PublicDependencyModuleNames.AddRange
		(
			new string[]
			{
				"Core",
				"CoreUObject",
				"ApplicationCore",
				"AudioMixer",
				"SignalProcessing",
				"InputCore",
				"Engine",
				"EditorFramework",
				"UnrealEd",
				"Slate",
				"SlateCore",
				
				"RenderCore",
				"LevelEditor",
				"Landscape",
				"PropertyEditor",
				"DetailCustomizations",
				"ClassViewer",
				"GraphEditor",
				"ContentBrowser",
			}
		);

		PrivateIncludePathModuleNames.AddRange
		(
			new string[]
			{
				"AssetTools",
				"WorkspaceMenuStructure",
			}
		);

		// Circular references that need to be cleaned up
		CircularlyReferencedDependentModules.AddRange
		(
			new string[]
			{
				"DetailCustomizations",
			}
		);

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
		{
			string PlatformName = "Win64";

			string LibSndFilePath = Target.UEThirdPartyBinariesDirectory + "libsndfile/";
			LibSndFilePath += PlatformName;


			PublicAdditionalLibraries.Add(LibSndFilePath + "/libsndfile-1.lib");
			PublicDelayLoadDLLs.Add("libsndfile-1.dll");
			PublicIncludePathModuleNames.Add("UELibSampleRate");

			RuntimeDependencies.Add("$(EngineDir)/Binaries/ThirdParty/libsndfile/" + PlatformName + "/libsndfile-1.dll");

			PublicDefinitions.Add("WITH_SNDFILE_IO=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_SNDFILE_IO=0");
		}
	}
}
