// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class AudioEditor : ModuleRules
{
	public AudioEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange
		(
			new string[]
			{
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
				"AssetDefinition",
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
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Apple))
		{
			string PlatformName = "Mac/";
			string LibFilename = "libsndfile.1.dylib";
			string LibFolder = "libsndfile/";
			string LibSndFilePath = Path.Combine(Target.UEThirdPartyBinariesDirectory, LibFolder, PlatformName, LibFilename);

			PublicDelayLoadDLLs.Add(LibSndFilePath);
			PublicIncludePathModuleNames.Add("UELibSampleRate");
			RuntimeDependencies.Add(LibSndFilePath);
			
			PublicDefinitions.Add("WITH_SNDFILE_IO=1");
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Linux))
		{
			string PlatformName = "Linux/";
			string LibFilename = "libsndfile.so.1";
			string LibFolder = "libsndfile/";
			string LibSndFilePath = Path.Combine(Target.UEThirdPartyBinariesDirectory, LibFolder, PlatformName, LibFilename);

			PublicDelayLoadDLLs.Add(LibSndFilePath);
			PublicIncludePathModuleNames.Add("UELibSampleRate");
			RuntimeDependencies.Add(LibSndFilePath);
			
			PublicDefinitions.Add("WITH_SNDFILE_IO=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_SNDFILE_IO=0");
		}
	}
}
