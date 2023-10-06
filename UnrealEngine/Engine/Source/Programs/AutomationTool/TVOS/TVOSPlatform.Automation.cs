// Copyright Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using AutomationTool;
using UnrealBuildTool;
using System.Text.RegularExpressions;
using EpicGames.Core;

public class TVOSPlatform : IOSPlatform
{
	public TVOSPlatform()
		:base(UnrealTargetPlatform.TVOS)
	{
		TargetIniPlatformType = UnrealTargetPlatform.IOS;
	}

	public override bool PrepForUATPackageOrDeploy(UnrealTargetConfiguration Config, FileReference ProjectFile, string InProjectName, DirectoryReference InProjectDirectory, FileReference Executable, DirectoryReference InEngineDir, bool bForDistribution, string CookFlavor, bool bIsDataDeploy, bool bCreateStubIPA, bool bIsUEGame)
	{
		string TargetName = Path.GetFileNameWithoutExtension(Executable.FullName).Split("-".ToCharArray())[0];
		FileReference TargetReceiptFileName;
		if (bIsUEGame)
		{
			TargetReceiptFileName = TargetReceipt.GetDefaultPath(InEngineDir, "UnrealGame", UnrealTargetPlatform.TVOS, Config, null);
		}
		else
		{
			TargetReceiptFileName = TargetReceipt.GetDefaultPath(InProjectDirectory, TargetName, UnrealTargetPlatform.TVOS, Config, null);
		}
		return TVOSExports.PrepForUATPackageOrDeploy(Config, ProjectFile, InProjectName, InProjectDirectory, Executable, InEngineDir, bForDistribution, CookFlavor, bIsDataDeploy, bCreateStubIPA, TargetReceiptFileName, Log.Logger);
	}

    public override void GetProvisioningData(FileReference InProject, bool bDistribution, out string MobileProvision, out string SigningCertificate, out string Team, out bool bAutomaticSigning)
    {
		TVOSExports.GetProvisioningData(InProject, bDistribution, out MobileProvision, out SigningCertificate, out Team, out bAutomaticSigning);
    }

	public override bool DeployGeneratePList(FileReference ProjectFile, UnrealTargetConfiguration Config, DirectoryReference ProjectDirectory, bool bIsUEGame, string GameName, bool bIsClient, string ProjectName, DirectoryReference InEngineDir, DirectoryReference AppDirectory, string InExecutablePath)
	{
		string TargetName = Path.GetFileNameWithoutExtension(InExecutablePath).Split("-".ToCharArray())[0];
		FileReference TargetReceiptFileName;
		if (bIsUEGame)
		{
			TargetReceiptFileName = TargetReceipt.GetDefaultPath(InEngineDir, "UnrealGame", UnrealTargetPlatform.TVOS, Config, null);
		}
		else
		{
			TargetReceiptFileName = TargetReceipt.GetDefaultPath(ProjectDirectory, TargetName, UnrealTargetPlatform.TVOS, Config, null);
		}
		return TVOSExports.GeneratePList(ProjectFile, Config, ProjectDirectory, bIsUEGame, GameName, bIsClient, ProjectName, InEngineDir, AppDirectory, TargetReceiptFileName, Log.Logger);
	}

    public override string GetCookPlatform(bool bDedicatedServer, bool bIsClientOnly)
	{
		return bIsClientOnly ? "TVOSClient" : "TVOS";
	}

    public override void GetFilesToDeployOrStage(ProjectParams Params, DeploymentContext SC)
    {
        //		if (UnrealBuildTool.BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
        {
            // copy the icons/launch screens from the engine
            {
				FileReference SourcePath = FileReference.Combine(SC.LocalRoot, "Engine", "Binaries", "TVOS", "AssetCatalog", "Assets.car");
				if(FileReference.Exists(SourcePath))
				{
					SC.StageFile(StagedFileType.SystemNonUFS, SourcePath, new StagedFileReference("Assets.car"));
				}
            }

            // copy any additional framework assets that will be needed at runtime
            {
                DirectoryReference SourcePath = DirectoryReference.Combine((SC.IsCodeBasedProject ? SC.ProjectRoot : SC.EngineRoot), "Intermediate", "TVOS", "FrameworkAssets");
                if (DirectoryReference.Exists(SourcePath))
                {
					SC.StageFiles(StagedFileType.SystemNonUFS, SourcePath, StageFilesSearch.AllDirectories, StagedDirectoryReference.Root);
                }
            }

            // copy the icons/launch screens from the game (may stomp the engine copies)
            {
                FileReference SourcePath = FileReference.Combine(SC.ProjectRoot, "Binaries", "TVOS", "AssetCatalog", "Assets.car");
				if(FileReference.Exists(SourcePath))
				{
	                SC.StageFile(StagedFileType.SystemNonUFS, SourcePath, new StagedFileReference("Assets.car"));
				}
            }

            // copy the plist (only if code signing, as it's protected by the code sign blob in the executable and can't be modified independently)
            if (GetCodeSignDesirability(Params))
            {
				// this would be FooClient when making a client-only build
				string TargetName = SC.StageExecutables[0].Split("-".ToCharArray())[0];
				DirectoryReference SourcePath = DirectoryReference.Combine((SC.IsCodeBasedProject ? SC.ProjectRoot : SC.EngineRoot), "Intermediate", "TVOS");
                FileReference TargetPListFile = FileReference.Combine(SourcePath, (SC.IsCodeBasedProject ? TargetName : "UnrealGame") + "-Info.plist");
                //				if (!File.Exists(TargetPListFile))
                {
                    // ensure the plist, entitlements, and provision files are properly copied
                    Console.WriteLine("CookPlat {0}, this {1}", GetCookPlatform(false, false), ToString());
                    if (!SC.IsCodeBasedProject)
                    {
                        UnrealBuildTool.PlatformExports.SetRemoteIniPath(SC.ProjectRoot.FullName);
                    }

                    if (SC.StageTargetConfigurations.Count != 1)
                    {
                        throw new AutomationException("iOS is currently only able to package one target configuration at a time, but StageTargetConfigurations contained {0} configurations", SC.StageTargetConfigurations.Count);
                    }

                    var TargetConfiguration = SC.StageTargetConfigurations[0];

					DirectoryReference ProjectRoot = SC.ProjectRoot;
					// keep old logic for BP projects with legacy
					if (!AppleExports.UseModernXcode(SC.RawProjectPath) && !SC.IsCodeBasedProject)
					{
						ProjectRoot = DirectoryReference.Combine(SC.LocalRoot, "Engine");
					}

					DeployGeneratePList(
						SC.RawProjectPath,
						TargetConfiguration,
						ProjectRoot,
						!SC.IsCodeBasedProject,
						(SC.IsCodeBasedProject ? TargetName : "UnrealGame"),
						Params.Client,
						SC.ShortProjectName,
						SC.EngineRoot,
						DirectoryReference.Combine((SC.IsCodeBasedProject ? SC.ProjectRoot : SC.EngineRoot),
						"Binaries",
						"TVOS",
						"Payload",
						(SC.IsCodeBasedProject ? SC.ShortProjectName : "UnrealGame") + ".app"),
						SC.StageExecutables[0]);
                }


                // copy the udebugsymbols if they exist
                {
                    ConfigHierarchy PlatformGameConfig;
                    bool bIncludeSymbols = false;
                    if (Params.EngineConfigs.TryGetValue(SC.StageTargetPlatform.PlatformType, out PlatformGameConfig))
                    {
                        PlatformGameConfig.GetBool("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bGenerateCrashReportSymbols", out bIncludeSymbols);
                    }
                    if (bIncludeSymbols)
                    {
                        FileReference SymbolFileName = FileReference.Combine((SC.IsCodeBasedProject ? SC.ProjectRoot : SC.EngineRoot), "Binaries", "TVOS", SC.StageExecutables[0] + ".udebugsymbols");
                        if (FileReference.Exists(SymbolFileName))
                        {
                            SC.StageFile(StagedFileType.NonUFS, SymbolFileName, new StagedFileReference((Params.ShortProjectName + ".udebugsymbols").ToLowerInvariant()));
                        }
                    }
                }

				if (!AppleExports.UseModernXcode(SC.RawProjectPath))
				{
					// copy the plist to the stage dir
					SC.StageFile(StagedFileType.SystemNonUFS, TargetPListFile, new StagedFileReference("Info.plist"));
				}
			}
		}

        // copy the movies from the project
        {
            StageMovieFiles(DirectoryReference.Combine(SC.EngineRoot, "Content", "Movies"), SC);
            StageMovieFiles(DirectoryReference.Combine(SC.ProjectRoot, "Content", "Movies"), SC);
        }

        {
            // Stage any *.metallib files as NonUFS.
            // Get the final output directory for cooked data
            DirectoryReference CookOutputDir;
            if (!String.IsNullOrEmpty(Params.CookOutputDir))
            {
                CookOutputDir = DirectoryReference.Combine(new DirectoryReference(Params.CookOutputDir), SC.CookPlatform);
            }
            else if (Params.CookInEditor)
            {
                CookOutputDir = DirectoryReference.Combine(SC.ProjectRoot, "Saved", "EditorCooked", SC.CookPlatform);
            }
            else
            {
                CookOutputDir = DirectoryReference.Combine(SC.ProjectRoot, "Saved", "Cooked", SC.CookPlatform);
            }
            if (DirectoryReference.Exists(CookOutputDir))
            {
                List<FileReference> CookedFiles = DirectoryReference.EnumerateFiles(CookOutputDir, "*.metallib", SearchOption.AllDirectories).ToList();
                foreach (FileReference CookedFile in CookedFiles)
                {
                    SC.StageFile(StagedFileType.NonUFS, CookedFile, new StagedFileReference(CookedFile.MakeRelativeTo(CookOutputDir)));
                }
            }
        }
    }
}
