// Copyright Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.IO;
using AutomationTool;
using UnrealBuildTool;
using UnrealBuildBase;
using EpicGames.Core;

public class MacPlatform : Platform
{
	/// <summary>
	/// Default architecture to build projects for. Defaults to Intel
	/// </summary>
	protected string[] ProjectTargetArchitectures = { MacExports.IntelArchitecture };

	public MacPlatform()
		: base(UnrealTargetPlatform.Mac)
	{

	}

	public override void PlatformSetupParams(ref ProjectParams ProjParams)
	{
		base.PlatformSetupParams(ref ProjParams);

		string ConfigTargetArchicture = "";
		ConfigHierarchy PlatformEngineConfig = null;
		if (ProjParams.EngineConfigs.TryGetValue(PlatformType, out PlatformEngineConfig))
		{
			PlatformEngineConfig.GetString("/Script/MacTargetPlatform.MacTargetSettings", "TargetArchitecture", out ConfigTargetArchicture);

			if (ConfigTargetArchicture.ToLower().Contains("intel"))
			{
				ProjectTargetArchitectures = new[] { MacExports.IntelArchitecture };
			}
			else if (ConfigTargetArchicture.ToLower().Contains("apple"))
			{
				ProjectTargetArchitectures = new[] { MacExports.AppleArchitecture};
			}
			else if (ConfigTargetArchicture.ToLower().Contains("universal"))
			{
				ProjectTargetArchitectures = new[] { MacExports.IntelArchitecture, MacExports.AppleArchitecture };
			}
		}		
	}

	public override DeviceInfo[] GetDevices()
	{
		List<DeviceInfo> Devices = new List<DeviceInfo>();

		if (HostPlatform.Current.HostEditorPlatform == TargetPlatformType)
		{
			DeviceInfo LocalMachine = new DeviceInfo(TargetPlatformType, Environment.MachineName, Environment.MachineName,
				Environment.OSVersion.Version.ToString(), "Computer", true, true);

			Devices.Add(LocalMachine);
		}

		return Devices.ToArray();
	}

	public override string GetCookPlatform(bool bDedicatedServer, bool bIsClientOnly)
	{
		const string NoEditorCookPlatform = "Mac";
		const string ServerCookPlatform = "MacServer";
		const string ClientCookPlatform = "MacClient";

		if (bDedicatedServer)
		{
			return ServerCookPlatform;
		}
		else if (bIsClientOnly)
		{
			return ClientCookPlatform;
		}
		else
		{
			return NoEditorCookPlatform;
		}
	}

	public override string GetEditorCookPlatform()
	{
		return "MacEditor";
	}

	/// <summary>
	/// Returns true if UAT can build this target for all Mac architectures
	/// </summary>
	/// <param name="InTarget"></param>
	/// <param name="InParams"></param>
	/// <returns></returns>
	protected bool CanBuildTargetForAllArchitectures(UnrealBuild.BuildTarget InTarget, ProjectParams InParams)
	{
		return MacExports.TargetsAllowedForAppleSilicon.Contains(InTarget.TargetName, StringComparer.OrdinalIgnoreCase);
	}

	/// <summary>
	/// Override PreBuildAgenda so we can control the architecture that targets are built for based on
	/// project settings and the current user environment
	/// </summary>
	/// <param name="Build"></param>
	/// <param name="Agenda"></param>
	/// <param name="Params"></param>
	public override void PreBuildAgenda(UnrealBuild Build, UnrealBuild.BuildAgenda Agenda, ProjectParams Params)
	{
		base.PreBuildAgenda(Build, Agenda, Params);

		string LocalArchitecture = MacExports.HostArchitecture;

		bool ProjectIsUniversal = ProjectTargetArchitectures.Count() > 1;

		// Go through the agenda for all targets and set the architecture appropriately
		foreach (UnrealBuild.BuildTarget Target in Agenda.Targets)
		{
			bool IsTarget = Params.ClientCookedTargets.Contains(Target.TargetName) || Params.ServerCookedTargets.Contains(Target.TargetName);

			// Default to Intel. 
			string UBTArchitectureParam = MacExports.IntelArchitecture;

			// Targets are easy. 
			// - If the project is set to Intel/Apple we do that
			// - If it's universal we build the local architecture unless distributing
			// - SpecifiedArchitecture overrides these
			if (IsTarget)
			{
				// If an architecture was specified, use that
				if (!string.IsNullOrEmpty(Params.SpecifiedArchitecture))
				{
					UBTArchitectureParam = Params.SpecifiedArchitecture;
					Log.TraceInformation("Building {0} as {1} due to -specifiedarchitecture", Target.TargetName, UBTArchitectureParam);
				}
				else
				{
					// If the project isn't marked as universal built what it's set to
					if (!ProjectIsUniversal)
					{
						UBTArchitectureParam = ProjectTargetArchitectures.First();
						Log.TraceInformation("Building {0} as {1}", Target.TargetName, UBTArchitectureParam);
					}
					else
					{
						// if it is universal, build everything for distribution or just the local architecture otherwise
						if (Params.Distribution)
						{
							UBTArchitectureParam = string.Join("+", ProjectTargetArchitectures);
							Log.TraceInformation("Building {0} as {1} for distribution", Target.TargetName, UBTArchitectureParam);
						}
						else
						{
							UBTArchitectureParam = LocalArchitecture;
							Log.TraceInformation("Building {0} as {1} for local non-distribution", Target.TargetName, UBTArchitectureParam);
						}
					}
				}
			}
			else
			{
				// We build tools for the local architecture if possible
				if (CanBuildTargetForAllArchitectures(Target, Params) || LocalArchitecture == MacExports.IntelArchitecture)
				{
					UBTArchitectureParam = LocalArchitecture;
					Log.TraceInformation("Building {0} as {1} for host", Target.TargetName, UBTArchitectureParam);
				}
				else if (MacExports.IsRunningOnAppleArchitecture)
				{
					// Tell them why to avoid confusion
					Log.TraceInformation("Building {0} as {1} (arm64 not currently supported)", Target.TargetName, UBTArchitectureParam);
					UBTArchitectureParam = MacExports.IntelArchitecture;
				}
			}
			
			// TODO - This needs to be handled in a more graceful way, however for now
			// when in an installedbuild just leave it to the default arch and don't add anything extra
			if (!Unreal.IsEngineInstalled())
			{
				Target.UBTArgs += string.Format(" -architecture={0}", UBTArchitectureParam);
			}
		}
	}

	private void StageAppBundle(DeploymentContext SC, DirectoryReference InPath, StagedDirectoryReference NewName)
	{
		// Files with DebugFileExtensions should always be DebugNonUFS
		List<string> DebugExtensions = GetDebugFileExtensions();
		if(DirectoryExists(InPath.FullName))
		{
			foreach (FileReference InputFile in DirectoryReference.EnumerateFiles(InPath, "*", SearchOption.AllDirectories))
			{
				StagedFileReference OutputFile = StagedFileReference.Combine(NewName, InputFile.MakeRelativeTo(InPath));
				StagedFileType FileType = DebugExtensions.Any(x => InputFile.HasExtension(x)) ? StagedFileType.DebugNonUFS : StagedFileType.NonUFS;
				SC.StageFile(FileType, InputFile, OutputFile);
			}
		}
	}

	public override void GetFilesToDeployOrStage(ProjectParams Params, DeploymentContext SC)
	{
		// Stage all the build products
		foreach (StageTarget Target in SC.StageTargets)
		{
			SC.StageBuildProductsFromReceipt(Target.Receipt, Target.RequireFilesExist, Params.bTreatNonShippingBinariesAsDebugFiles);
		}

		if (SC.bStageCrashReporter)
		{
			StagedDirectoryReference CrashReportClientPath = StagedDirectoryReference.Combine("Engine/Binaries", SC.PlatformDir, "CrashReportClient.app");
			StageAppBundle(SC, DirectoryReference.Combine(SC.LocalRoot, "Engine/Binaries", SC.PlatformDir, "CrashReportClient.app"), CrashReportClientPath);
		}

		// Find the app bundle path
		List<FileReference> Exes = GetExecutableNames(SC);
		foreach (var Exe in Exes)
		{
			StagedDirectoryReference AppBundlePath = null;
			if (Exe.IsUnderDirectory(DirectoryReference.Combine(SC.RuntimeProjectRootDir, "Binaries", SC.PlatformDir)))
			{
				AppBundlePath = StagedDirectoryReference.Combine(SC.ShortProjectName, "Binaries", SC.PlatformDir, Path.GetFileNameWithoutExtension(Exe.FullName) + ".app");
			}
			else if (Exe.IsUnderDirectory(DirectoryReference.Combine(SC.RuntimeRootDir, "Engine/Binaries", SC.PlatformDir)))
			{
				AppBundlePath = StagedDirectoryReference.Combine("Engine/Binaries", SC.PlatformDir, Path.GetFileNameWithoutExtension(Exe.FullName) + ".app");
			}

			// Copy the custom icon and Steam dylib, if needed
			if (AppBundlePath != null)
			{
				FileReference AppIconsFile = FileReference.Combine(SC.ProjectRoot, "Build", "Mac", "Application.icns");
				if(FileReference.Exists(AppIconsFile))
				{
					SC.StageFile(StagedFileType.NonUFS, AppIconsFile, StagedFileReference.Combine(AppBundlePath, "Contents", "Resources", "Application.icns"));
				}
			}
		}

		// Copy the splash screen, Mac specific
		FileReference SplashImage = FileReference.Combine(SC.ProjectRoot, "Content", "Splash", "Splash.bmp");
		if(FileReference.Exists(SplashImage))
		{
			SC.StageFile(StagedFileType.NonUFS, SplashImage);
		}

		// Stage the bootstrap executable
		if (!Params.NoBootstrapExe)
		{
			foreach (StageTarget Target in SC.StageTargets)
			{
				BuildProduct Executable = Target.Receipt.BuildProducts.FirstOrDefault(x => x.Type == BuildProductType.Executable);
				if (Executable != null)
				{
					// only create bootstraps for executables
					List<StagedFileReference> StagedFiles = SC.FilesToStage.NonUFSFiles.Where(x => x.Value == Executable.Path).Select(x => x.Key).ToList();
					if (StagedFiles.Count > 0 && Executable.Path.FullName.Replace("\\", "/").Contains("/" + TargetPlatformType.ToString() + "/"))
					{
						string BootstrapArguments = "";
						if (!ShouldStageCommandLine(Params, SC))
						{
							if (!SC.IsCodeBasedProject)
							{
								BootstrapArguments = String.Format("../../../{0}/{0}.uproject", SC.ShortProjectName);
							}
							else
							{
								BootstrapArguments = SC.ShortProjectName;
							}
						}

						string BootstrapExeName;
						if (SC.StageTargetConfigurations.Count > 1)
						{
							BootstrapExeName = Path.GetFileName(Executable.Path.FullName) + ".app";
						}
						else if (Params.IsCodeBasedProject)
						{
							// We want Mac-Shipping etc in the bundle name
							BootstrapExeName = Path.GetFileName(Executable.Path.FullName) + ".app";
						}
						else
						{
							BootstrapExeName = SC.ShortProjectName + ".app";
						}

						string AppSuffix = ".app" + Path.DirectorySeparatorChar;

						string AppPath = Executable.Path.FullName.Substring(0, Executable.Path.FullName.LastIndexOf(AppSuffix) + AppSuffix.Length);
						foreach (var DestPath in StagedFiles)
						{
							string AppRelativePath = DestPath.Name.Substring(0, DestPath.Name.LastIndexOf(AppSuffix) + AppSuffix.Length);
							StageBootstrapExecutable(SC, BootstrapExeName, AppPath, AppRelativePath, BootstrapArguments);
						}
					}
				}
			}
		}

		// Copy the ShaderCache files, if they exist
		FileReference DrawCacheFile = FileReference.Combine(SC.ProjectRoot, "Content", "DrawCache.ushadercache");
		if(FileReference.Exists(DrawCacheFile))
		{
			SC.StageFile(StagedFileType.UFS, DrawCacheFile);
		}

		FileReference ByteCodeCacheFile = FileReference.Combine(SC.ProjectRoot, "Content", "ByteCodeCache.ushadercode");
		if(FileReference.Exists(ByteCodeCacheFile))
		{
			SC.StageFile(StagedFileType.UFS, ByteCodeCacheFile);
		}

		{
			// Stage any *.metallib files as NonUFS.
			// Get the final output directory for cooked data
			DirectoryReference CookOutputDir;
			if(!String.IsNullOrEmpty(Params.CookOutputDir))
			{
				CookOutputDir = DirectoryReference.Combine(new DirectoryReference(Params.CookOutputDir), SC.CookPlatform);
			}
			else if(Params.CookInEditor)
			{
				CookOutputDir = DirectoryReference.Combine(SC.ProjectRoot, "Saved", "EditorCooked", SC.CookPlatform);
			}
			else
			{
				CookOutputDir = DirectoryReference.Combine(SC.ProjectRoot, "Saved", "Cooked", SC.CookPlatform);
			}
			if(DirectoryReference.Exists(CookOutputDir))
			{
				List<FileReference> CookedFiles = DirectoryReference.EnumerateFiles(CookOutputDir, "*.metallib", SearchOption.AllDirectories).ToList();
				foreach(FileReference CookedFile in CookedFiles)
				{
					SC.StageFile(StagedFileType.NonUFS, CookedFile, new StagedFileReference(CookedFile.MakeRelativeTo(CookOutputDir)));
				}
			}
		}
	}

	string GetValueFromInfoPlist(string InfoPlist, string Key, string DefaultValue = "")
	{
		string Value = DefaultValue;
		string KeyString = "<key>" + Key + "</key>";
		int KeyIndex = InfoPlist.IndexOf(KeyString);
		if (KeyIndex > 0)
		{
			int ValueStartIndex = InfoPlist.IndexOf("<string>", KeyIndex + KeyString.Length) + "<string>".Length;
			int ValueEndIndex = InfoPlist.IndexOf("</string>", ValueStartIndex);
			if (ValueStartIndex > 0 && ValueEndIndex > ValueStartIndex)
			{
				Value = InfoPlist.Substring(ValueStartIndex, ValueEndIndex - ValueStartIndex);
			}
		}
		return Value;
	}

	void StageBootstrapExecutable(DeploymentContext SC, string ExeName, string TargetFile, string StagedRelativeTargetPath, string StagedArguments)
	{
		DirectoryReference InputApp = DirectoryReference.Combine(SC.LocalRoot, "Engine", "Binaries", SC.PlatformDir, "BootstrapPackagedGame.app");
		if (InternalUtils.SafeDirectoryExists(InputApp.FullName))
		{
			// Create the new bootstrap program
			DirectoryReference IntermediateDir = DirectoryReference.Combine(SC.ProjectRoot, "Intermediate", "Staging");
			InternalUtils.SafeCreateDirectory(IntermediateDir.FullName);

			DirectoryReference IntermediateApp = DirectoryReference.Combine(IntermediateDir, ExeName);
			if (DirectoryReference.Exists(IntermediateApp))
			{
				DirectoryReference.Delete(IntermediateApp, true);
			}
			CloneDirectory(InputApp.FullName, IntermediateApp.FullName);

			// Rename the executable
			string GameName = Path.GetFileNameWithoutExtension(ExeName);
			FileReference.Move(FileReference.Combine(IntermediateApp, "Contents", "MacOS", "BootstrapPackagedGame"), FileReference.Combine(IntermediateApp, "Contents", "MacOS", GameName));

			// Copy the icon
			string SrcInfoPlistPath = CombinePaths(TargetFile, "Contents", "Info.plist");
			string SrcInfoPlist = File.ReadAllText(SrcInfoPlistPath);

			string IconName = GetValueFromInfoPlist(SrcInfoPlist, "CFBundleIconFile");
			if (!string.IsNullOrEmpty(IconName))
			{
				string IconPath = CombinePaths(TargetFile, "Contents", "Resources", IconName + ".icns");
				InternalUtils.SafeCreateDirectory(CombinePaths(IntermediateApp.FullName, "Contents", "Resources"));
				File.Copy(IconPath, CombinePaths(IntermediateApp.FullName, "Contents", "Resources", IconName + ".icns"));
			}

			// Update Info.plist contents
			string DestInfoPlistPath = CombinePaths(IntermediateApp.FullName, "Contents", "Info.plist");
			string DestInfoPlist = File.ReadAllText(DestInfoPlistPath);

			string AppIdentifier = GetValueFromInfoPlist(SrcInfoPlist, "CFBundleIdentifier");
			if (AppIdentifier == "com.epicgames.UnrealGame")
			{
				AppIdentifier = "";
			}

			string Copyright = GetValueFromInfoPlist(SrcInfoPlist, "NSHumanReadableCopyright");
			string BundleVersion = GetValueFromInfoPlist(SrcInfoPlist, "CFBundleVersion", "1");
			string ShortVersion = GetValueFromInfoPlist(SrcInfoPlist, "CFBundleShortVersionString", "1.0");

			DestInfoPlist = DestInfoPlist.Replace("com.epicgames.BootstrapPackagedGame", string.IsNullOrEmpty(AppIdentifier) ? "com.epicgames." + GameName + "_bootstrap" : AppIdentifier + "_bootstrap");
			DestInfoPlist = DestInfoPlist.Replace("BootstrapPackagedGame", GameName);
			DestInfoPlist = DestInfoPlist.Replace("__UE4_ICON_FILE__", IconName);
			DestInfoPlist = DestInfoPlist.Replace("__UE4_APP_TO_LAUNCH__", StagedRelativeTargetPath);
			DestInfoPlist = DestInfoPlist.Replace("__UE4_COMMANDLINE__", StagedArguments);
			DestInfoPlist = DestInfoPlist.Replace("__UE4_COPYRIGHT__", Copyright);
			DestInfoPlist = DestInfoPlist.Replace("__UE4_BUNDLE_VERSION__", BundleVersion);
			DestInfoPlist = DestInfoPlist.Replace("__UE4_SHORT_VERSION__", ShortVersion);

			File.WriteAllText(DestInfoPlistPath, DestInfoPlist);

			StageAppBundle(SC, IntermediateApp, new StagedDirectoryReference(ExeName));
		}
	}

	private void RemoveExtraRPaths(ProjectParams Params, DeploymentContext SC)
	{
		// When we link the executable we add RPATH entries for all possible places where dylibs can be loaded from, so that the same executable can be used from Binaries/Mac
		// as well as in a packaged, self-contained application. In recent versions of macOS, Gatekeeper doesn't allow RPATHs pointing to folders that don't exist,
		// so we remove these based on the type of packaging (Params.CreateAppBundle).
		List<FileReference> Exes = GetExecutableNames(SC);
		foreach (var ExePath in Exes)
		{
			IProcessResult CommandResult = Run("otool", "-l \"" + ExePath + "\"", null, ERunOptions.None);
			if (CommandResult.ExitCode == 0)
			{
				bool bModifiedWithInstallNameTool = false;
				StringReader Reader = new StringReader(CommandResult.Output);
				Regex RPathPattern = new Regex(@"^\s+path (?<rpath>.+)\s\(offset");
				string ToRemovePattern = Params.CreateAppBundle ? "/../../../" : "@loader_path/../UE4/";

				string OutputLine;
				while ((OutputLine = Reader.ReadLine()) != null)
				{
					if (OutputLine.EndsWith("cmd LC_RPATH"))
					{
						OutputLine = Reader.ReadLine();
						OutputLine = Reader.ReadLine();
						Match RPathMatch = RPathPattern.Match(OutputLine);
						if (RPathMatch.Success)
						{
							string RPath = RPathMatch.Groups["rpath"].Value;
							if (RPath.Contains(ToRemovePattern))
							{
								Run("xcrun", "install_name_tool -delete_rpath \"" + RPath + "\" \"" + ExePath + "\"", null, ERunOptions.NoStdOutCapture);
								bModifiedWithInstallNameTool = true;
							}
						}
					}
				}

				// Now re-sign the exectuables if they were previously signed during staging. Modifying the binaries
				// via install_name_tool _after_ signing them invalidates the signature.
				if (bModifiedWithInstallNameTool && (Params.bCodeSign && !Params.SkipStage))
				{
					SC.StageTargetPlatform.SignExecutables(SC, Params);
				}
			}
		}
	}

	public override void Package(ProjectParams Params, DeploymentContext SC, int WorkingCL)
	{
		// package up the program, potentially with an installer for Mac
		PrintRunTime();
	}

	public override void ProcessArchivedProject(ProjectParams Params, DeploymentContext SC)
	{
		if (Params.CreateAppBundle)
		{
			string ExeName = SC.StageExecutables[0];
			string BundlePath = SC.IsCodeBasedProject ? CombinePaths(SC.ArchiveDirectory.FullName, ExeName + ".app") : BundlePath = CombinePaths(SC.ArchiveDirectory.FullName, SC.ShortProjectName + ".app");

			if (SC.bIsCombiningMultiplePlatforms)
			{
				// when combining multiple platforms, don't merge the content into the .app, use the one in the Binaries directory
				BundlePath = CombinePaths(SC.ArchiveDirectory.FullName, SC.ShortProjectName, "Binaries", "Mac", ExeName + ".app");
				if (!DirectoryExists(BundlePath))
				{
					// if the .app wasn't there, just skip out (we don't require executables when combining)
					return;
				}
			}

			string TargetPath = CombinePaths(BundlePath, "Contents", "UE");
			if (!SC.bIsCombiningMultiplePlatforms)
			{
				DeleteDirectory(true, BundlePath);

				string SourceBundlePath = CombinePaths(SC.ArchiveDirectory.FullName, SC.ShortProjectName, "Binaries", "Mac", ExeName + ".app");
				if (!DirectoryExists(SourceBundlePath))
				{
					SourceBundlePath = CombinePaths(SC.ArchiveDirectory.FullName, "Engine", "Binaries", "Mac", ExeName + ".app");
					if (!DirectoryExists(SourceBundlePath))
					{
						SourceBundlePath = CombinePaths(SC.ArchiveDirectory.FullName, "Engine", "Binaries", "Mac", "UE4.app");
					}
				}
				RenameDirectory(SourceBundlePath, BundlePath, true);

				DeleteDirectory(true, TargetPath);

				string[] StagedFiles = Directory.GetFiles(SC.ArchiveDirectory.FullName, "*", SearchOption.TopDirectoryOnly);
				foreach (string FilePath in StagedFiles)
				{
					string TargetFilePath = CombinePaths(TargetPath, Path.GetFileName(FilePath));
					CreateDirectory(Path.GetDirectoryName(TargetFilePath));
					RenameFile(FilePath, TargetFilePath, true);
				}

				string[] StagedDirectories = Directory.GetDirectories(SC.ArchiveDirectory.FullName, "*", SearchOption.TopDirectoryOnly);
				foreach (string DirPath in StagedDirectories)
				{
					string DirName = Path.GetFileName(DirPath);
					if (!DirName.EndsWith(".app"))
					{
						string TargetDirPath = CombinePaths(TargetPath, DirName);
						CreateDirectory(Path.GetDirectoryName(TargetDirPath));
						RenameDirectory(DirPath, TargetDirPath, true);
					}
				}
			}

			// Update executable name, icon and entry in Info.plist
			string UE4GamePath = CombinePaths(BundlePath, "Contents", "MacOS", ExeName);
			if (!SC.IsCodeBasedProject && ExeName != SC.ShortProjectName && FileExists(UE4GamePath))
			{
				string GameExePath = CombinePaths(BundlePath, "Contents", "MacOS", SC.ShortProjectName);
				DeleteFile(GameExePath);
				RenameFile(UE4GamePath, GameExePath);

				string DefaultIconPath = CombinePaths(BundlePath, "Contents", "Resources", "UnrealGame.icns");
				string CustomIconSrcPath = CombinePaths(BundlePath, "Contents", "Resources", "Application.icns");
				string CustomIconDestPath = CombinePaths(BundlePath, "Contents", "Resources", SC.ShortProjectName + ".icns");
				if (FileExists(CustomIconSrcPath))
				{
					DeleteFile(DefaultIconPath);
					DeleteFile(CustomIconDestPath);
					RenameFile(CustomIconSrcPath, CustomIconDestPath, true);
				}
				else if (FileExists(DefaultIconPath))
				{
					DeleteFile(CustomIconDestPath);
					RenameFile(DefaultIconPath, CustomIconDestPath, true);
				}

				string InfoPlistPath = CombinePaths(BundlePath, "Contents", "Info.plist");
				string InfoPlistContents = File.ReadAllText(InfoPlistPath);
				InfoPlistContents = InfoPlistContents.Replace(ExeName, SC.ShortProjectName);
				InfoPlistContents = InfoPlistContents.Replace("<string>UnrealGame</string>", "<string>" + SC.ShortProjectName + "</string>");
				DeleteFile(InfoPlistPath);
				WriteAllText(InfoPlistPath, InfoPlistContents);
			}

			if (!SC.bIsCombiningMultiplePlatforms)
			{
				// creating these directories when the content isn't moved into the application causes it 
				// to fail to load, and isn't needed
				CreateDirectory(CombinePaths(TargetPath, "Engine", "Binaries", "Mac"));
				CreateDirectory(CombinePaths(TargetPath, SC.ShortProjectName, "Binaries", "Mac"));
			}

			// Find any dSYM files in the Manifest_DebugFiles_Mac file, move them to the archive directory, and remove them from the manifest.
			string[] DebugManifests = FindFiles("Manifest_DebugFiles_Mac.txt", true, SC.ArchiveDirectory.FullName);
			if ( DebugManifests.Count() > 0 )
			{
				string DebugManifest=DebugManifests[0];

				List<string> ManifestLines = new List<string>(File.ReadAllLines(DebugManifest));
				bool ModifyManifest = false;
				for (int ManifestLineIndex = ManifestLines.Count - 1; ManifestLineIndex >= 0; ManifestLineIndex--)
				{
					string ManifestLine = ManifestLines[ManifestLineIndex];
					int TabIndex = ManifestLine.IndexOf('\t');
					if (TabIndex > 0)
					{
						string FoundDebugFile = ManifestLine.Substring(0, TabIndex);
						if (FoundDebugFile.Contains(".dSYM"))
						{
							FoundDebugFile = CombinePaths(TargetPath, FoundDebugFile);
							string MovedDebugFile = CombinePaths(SC.ArchiveDirectory.FullName, Path.GetFileName(FoundDebugFile));

							RenameFile(FoundDebugFile, MovedDebugFile);

							Log.TraceInformation("Moving debug file: '{0}')", FoundDebugFile);
							ManifestLines.RemoveAt(ManifestLineIndex);
							ModifyManifest = true;
						}
					}
				}
				if (ModifyManifest)
				{
					File.WriteAllLines(DebugManifest, ManifestLines.ToArray());
				}
			}

			// If there is a dSYM matching the exe name rename it so it matches the project name
			string ExeDSYMName = CombinePaths(SC.ArchiveDirectory.FullName, ExeName + ".dSYM");
			string ProjectDSYMName = CombinePaths(SC.ArchiveDirectory.FullName, SC.ShortProjectName + ".dSYM");

			if (ExeDSYMName != ProjectDSYMName)
			{
				if (FileExists(ExeDSYMName))
				{
					RenameFile(ExeDSYMName, ProjectDSYMName);
				}
			}

			Run("/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister", "-f " + BundlePath, null, ERunOptions.Default);
		}
	}

	public override IProcessResult RunClient(ERunOptions ClientRunFlags, string ClientApp, string ClientCmdLine, ProjectParams Params)
	{
		if (!File.Exists(ClientApp))
		{
			if (Directory.Exists(ClientApp + ".app"))
			{
				ClientApp += ".app/Contents/MacOS/" + Path.GetFileName(ClientApp);
			}
			else
			{
				Int32 BaseDirLen = Params.BaseStageDirectory.Length;
				string StageSubDir = ClientApp.Substring(BaseDirLen, ClientApp.IndexOf("/", BaseDirLen + 1) - BaseDirLen);
				ClientApp = CombinePaths(Params.BaseStageDirectory, StageSubDir, Params.ShortProjectName + ".app/Contents/MacOS/" + Params.ShortProjectName);
			}
		}

		PushDir(Path.GetDirectoryName(ClientApp));
		// Always start client process and don't wait for exit.
		IProcessResult ClientProcess = Run(ClientApp, ClientCmdLine, null, ClientRunFlags | ERunOptions.NoWaitForExit);
		PopDir();

		return ClientProcess;
	}

	public override bool IsSupported { get { return true; } }

	public override List<string> GetDebugFileExtensions()
	{
		return new List<string> { ".dSYM" };
	}
	public override bool CanHostPlatform(UnrealTargetPlatform Platform)
	{
		if (Platform == UnrealTargetPlatform.IOS || Platform == UnrealTargetPlatform.Mac || Platform == UnrealTargetPlatform.TVOS)
		{
			return true;
		}
		return false;
	}

	public override bool ShouldStageCommandLine(ProjectParams Params, DeploymentContext SC)
	{
		return false; // !String.IsNullOrEmpty(Params.StageCommandline) || !String.IsNullOrEmpty(Params.RunCommandline) || (!Params.IsCodeBasedProject && Params.NoBootstrapExe);
	}

	public override bool SignExecutables(DeploymentContext SC, ProjectParams Params)
	{
		if (UnrealBuildTool.BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
		{
			if (Params.Archive)
			{
				// Remove extra RPATHs if we will be archiving the project
				LogInformation("Removing extraneous rpath entries");
				RemoveExtraRPaths(Params, SC);
			}

			// Sign everything we built
			List<FileReference> FilesToSign = GetExecutableNames(SC);
			LogInformation("RuntimeProjectRootDir: " + SC.RuntimeProjectRootDir);
			foreach (var Exe in FilesToSign)
			{
				LogInformation("Signing: " + Exe);
				string AppBundlePath = "";
				if (Exe.IsUnderDirectory(DirectoryReference.Combine(SC.RuntimeProjectRootDir, "Binaries", SC.PlatformDir)))
				{
					LogInformation("Starts with Binaries");
					AppBundlePath = CombinePaths(SC.RuntimeProjectRootDir.FullName, "Binaries", SC.PlatformDir, Path.GetFileNameWithoutExtension(Exe.FullName) + ".app");
				}
				else if (Exe.IsUnderDirectory(DirectoryReference.Combine(SC.RuntimeRootDir, "Engine/Binaries", SC.PlatformDir)))
				{
					LogInformation("Starts with Engine/Binaries");
					AppBundlePath = CombinePaths("Engine/Binaries", SC.PlatformDir, Path.GetFileNameWithoutExtension(Exe.FullName) + ".app");
				}

				LogInformation("Signing: " + AppBundlePath);
				CodeSign.SignMacFileOrFolder(AppBundlePath);
			}

		}
		return true;
	}

	public override void StripSymbols(FileReference SourceFile, FileReference TargetFile)
	{
		MacExports.StripSymbols(SourceFile, TargetFile, Log.Logger);
	}
}
