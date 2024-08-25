// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	class MacToolChainSettings : AppleToolChainSettings
	{
		/// <summary>
		/// Which version of the Mac OS X to allow at run time
		/// </summary>
		public string MinMacBuildVersion(TargetType TargetType)
		{
			return ((ApplePlatformSDK)UEBuildPlatformSDK.GetSDKForPlatform("Mac")!).GetBuildTargetVersion(TargetType);
		}

		/// <summary>
		/// Minimum version of Mac OS X to actually run on, running on earlier versions will display the system minimum version error dialog and exit.
		/// </summary>
		public string MinMacDeploymentVersion(TargetType TargetType)
		{
			return ((ApplePlatformSDK)UEBuildPlatformSDK.GetSDKForPlatform("Mac")!).GetDeploymentTargetVersion(TargetType);
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="bVerbose">Whether to output verbose logging</param>
		/// <param name="Logger">Logger for output</param>
		public MacToolChainSettings(bool bVerbose, ILogger Logger) 
			: base("MacOSX", null, "macos", bVerbose, Logger)
		{
		}

		public DirectoryReference GetSDKPath()
		{
			return GetSDKPath(UnrealArch.Host.Value);
		}
	}

	/// <summary>
	/// Mac toolchain wrapper
	/// </summary>
	class MacToolChain : AppleToolChain
	{
		public MacToolChain(ReadOnlyTargetRules? Target, ClangToolChainOptions InOptions, ILogger InLogger)
			: base(Target, () => new MacToolChainSettings(false, InLogger), InOptions, InLogger)
		{
		}

		public static MacToolChainSettings Settings => new MacToolChainSettings(false, Log.Logger);

		/// <summary>
		/// Which compiler\linker frontend to use
		/// </summary>
		private const string MacCompiler = "clang++";

		/// <summary>
		/// Which archiver to use
		/// </summary>
		private const string MacArchiver = "libtool";

		protected override ClangToolChainInfo GetToolChainInfo()
		{
			FileReference CompilerPath = FileReference.Combine(Settings.ToolchainDir, MacCompiler);
			FileReference ArchiverPath = FileReference.Combine(Settings.ToolchainDir, MacArchiver);
			return new AppleToolChainInfo(CompilerPath, ArchiverPath, Logger);
		}

		public static DirectoryReference FindProductDirectory(FileReference? ProjectFile, DirectoryReference BinaryDir, string? NameIfProgram)
		{
			// a project file is always used if there is one
			if (ProjectFile != null)
			{
				return ProjectFile.Directory;
			}
			// look to see if this is a program, and hunt down where the Programs directory is
			if (NameIfProgram != null)
			{
				return FindProgramDirectoryFromSource(BinaryDir, NameIfProgram, true) ?? Unreal.EngineDirectory;
			}
			return Unreal.EngineDirectory;
		}

		public static DirectoryReference? FindProgramDirectoryFromSource(DirectoryReference StartingDir, string ProgramName, bool bCreateIfNotFound)
		{
			DirectoryReference? ProgramFinder = StartingDir;
			while (ProgramFinder != null &&
				String.Compare(ProgramFinder.GetDirectoryName(), "Source", true) != 0 &&
				String.Compare(ProgramFinder.GetDirectoryName(), "Intermediate", true) != 0 &&
				String.Compare(ProgramFinder.GetDirectoryName(), "Binaries", true) != 0)
			{
				ProgramFinder = ProgramFinder.ParentDirectory;
			}

			// we are now at Source or similar directory, go up one more, then into Programs, and finally the "project" directory
			if (ProgramFinder != null)
			{
				DirectoryReference ProgramsDir = DirectoryReference.Combine(ProgramFinder, "../Programs");
				// if it doesn't exist, something went wrong, and we can't use this. throw an exception to catch it, for now
				if (!DirectoryReference.Exists(ProgramsDir))
				{
					throw new BuildException($"Unable to find Programs directory for {ProgramName}, when starting in {StartingDir}");
				}

				ProgramFinder = DirectoryReference.Combine(ProgramsDir, ProgramName);
				// if it exists, we have a ProductDir we can use for plists, icons, etc
				if (!DirectoryReference.Exists(ProgramFinder))
				{
					if (bCreateIfNotFound)
					{
						DirectoryReference.CreateDirectory(ProgramFinder);
						return ProgramFinder;
					}
				}
				else
				{
					return ProgramFinder;
				}
			}

			return null;
		}

		/// <inheritdoc/>
		protected override void GetCompileArguments_WarningsAndErrors(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			base.GetCompileArguments_WarningsAndErrors(CompileEnvironment, Arguments);

			//Arguments.Add("-Wsign-compare"); // fed up of not seeing the signed/unsigned warnings we get on Windows - lets enable them here too.
		}

		/// <inheritdoc/>
		protected override void GetCompileArguments_Debugging(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			base.GetCompileArguments_Debugging(CompileEnvironment, Arguments);

			// TODO: Mac always enables exceptions, is this correct?
			if (!CompileEnvironment.bEnableExceptions)
			{
				Arguments.Remove("-fno-exceptions");
				Arguments.Remove("-DPLATFORM_EXCEPTIONS_DISABLED=1");
			}
			Arguments.Add("-fexceptions");
			Arguments.Add("-DPLATFORM_EXCEPTIONS_DISABLED=0");

			if (CompileEnvironment.bHideSymbolsByDefault)
			{
				Arguments.Add("-fvisibility-ms-compat");
				Arguments.Add("-fvisibility-inlines-hidden");
			}
		}

		/// <inheritdoc/>
		protected override void GetCompileArguments_AdditionalArgs(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			if (!String.IsNullOrWhiteSpace(CompileEnvironment.AdditionalArguments))
			{
				string EscapedAdditionalArgs = String.Empty;
				foreach (string AdditionalArg in CompileEnvironment.AdditionalArguments.Split(new char[] { ' ' }, StringSplitOptions.RemoveEmptyEntries))
				{
					Match DefinitionMatch = Regex.Match(AdditionalArg, "-D\"?(?<Name>.*)=(?<Value>.*)\"?");
					if (DefinitionMatch.Success)
					{
						EscapedAdditionalArgs += String.Format(" -D{0}=\"{1}\"", DefinitionMatch.Groups["Name"].Value, DefinitionMatch.Groups["Value"].Value);
					}
					else
					{
						EscapedAdditionalArgs += " " + AdditionalArg;
					}
				}

				if (!String.IsNullOrWhiteSpace(EscapedAdditionalArgs))
				{
					Arguments.Add(EscapedAdditionalArgs);
				}
			}
		}

		/// <inheritdoc/>
		protected override void GetCompileArguments_Global(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			base.GetCompileArguments_Global(CompileEnvironment, Arguments);

			Arguments.Add("-fasm-blocks");

			// Disable FMA contraction on arm builds in order to preserve consistency with x64
			// derived data builds.
			// This is also added to the x64 builds in case someone ever adds -mfma.
			Arguments.Add("-ffp-contract=off");

			if (CompileEnvironment.bEnableOSX109Support)
			{
				Arguments.Add("-faligned-new"); // aligned operator new is supported only on macOS 10.14 and above
			}

			// Pass through architecture and OS info
			Arguments.Add("" + FormatArchitectureArg(CompileEnvironment.Architectures));
			Arguments.Add($"-isysroot \"{Settings.GetSDKPath()}\"");
			Arguments.Add("-mmacosx-version-min=" + (CompileEnvironment.bEnableOSX109Support ? "10.9" : Settings.MinMacBuildVersion(Target!.Type)));

			List<string> FrameworksSearchPaths = new List<string>();
			foreach (UEBuildFramework Framework in CompileEnvironment.AdditionalFrameworks)
			{
				FileReference FrameworkPath = new FileReference(Path.GetFullPath(Framework.Name));
				if (!FrameworksSearchPaths.Contains(FrameworkPath.Directory.FullName))
				{
					Arguments.Add($"-F \"{NormalizeCommandLinePath(FrameworkPath.Directory)}\"");
					FrameworksSearchPaths.Add(FrameworkPath.Directory.FullName);
				}
			}
		}

		string AddFrameworkToLinkCommand(string FrameworkName, string Arg = "-framework")
		{
			string Result = "";
			if (FrameworkName.EndsWith(".framework"))
			{
				Result += " -F \"" + Path.GetDirectoryName(Path.GetFullPath(FrameworkName)) + "\"";
				FrameworkName = Path.GetFileNameWithoutExtension(FrameworkName);
			}
			Result += " " + Arg + " \"" + FrameworkName + "\"";
			return Result;
		}

		protected override void GetLinkArguments_Global(LinkEnvironment LinkEnvironment, List<string> Arguments)
		{
			base.GetLinkArguments_Global(LinkEnvironment, Arguments);
			// Pass through architecture and OS info		
			Arguments.Add(FormatArchitectureArg(LinkEnvironment.Architectures));
			Arguments.Add(String.Format("-isysroot \"{0}\"", ToolChainSettings.Value.GetSDKPath(LinkEnvironment.Architecture)));
			Arguments.Add("-mmacosx-version-min=" + Settings.MinMacBuildVersion(Target!.Type));
			Arguments.Add("-dead_strip");

			// Temporary workaround for linker warning with Xcode 14:
			//		'ld: warning: could not create compact unwind for _inflate_fast: registers 27 not saved contiguously in frame'
			if (CompilerVersionLessThan(14, 0, 0))
			{
				Arguments.Add("-Wl,-fatal_warnings");
			}

			if (Options.HasFlag(ClangToolChainOptions.EnableAddressSanitizer) ||
				Options.HasFlag(ClangToolChainOptions.EnableThreadSanitizer) ||
				Options.HasFlag(ClangToolChainOptions.EnableUndefinedBehaviorSanitizer) ||
				Options.HasFlag(ClangToolChainOptions.EnableLibFuzzer))
			{
				Arguments.Add("-g");
				if (Options.HasFlag(ClangToolChainOptions.EnableAddressSanitizer))
				{
					Arguments.Add("-fsanitize=address");
				}
				else if (Options.HasFlag(ClangToolChainOptions.EnableThreadSanitizer))
				{
					Arguments.Add("-fsanitize=thread");
				}
				else if (Options.HasFlag(ClangToolChainOptions.EnableUndefinedBehaviorSanitizer))
				{
					Arguments.Add("-fsanitize=undefined");
				}
				if (Options.HasFlag(ClangToolChainOptions.EnableLibFuzzer))
				{
					Arguments.Add("-fsanitize=fuzzer");
				}
			}

			if (LinkEnvironment.bIsBuildingDLL)
			{
				Arguments.Add("-dynamiclib");
			}

			if (LinkEnvironment.Configuration == CppConfiguration.Debug)
			{
				// Apple's Clang is not supposed to run the de-duplication pass when linking in debug configs. Xcode adds this flag automatically, we need it as well, otherwise linking would take very long
				Arguments.Add("-Wl,-no_deduplicate");
			}

			// Needed to make sure install_name_tool will be able to update paths in Mach-O headers
			Arguments.Add("-headerpad_max_install_names");
		}

		void GetArchiveArguments_Global(LinkEnvironment LinkEnvironment, List<string> Arguments)
		{
			Arguments.Add("-static");
		}

		private void AppendMacLine(StreamWriter Writer, string Format, params object[] Arg)
		{
			string PreLine = String.Format(Format, Arg);
			Writer.Write(PreLine + "\n");
		}

		private int LoadEngineCL()
		{
			BuildVersion? Version;
			if (BuildVersion.TryRead(BuildVersion.GetDefaultFileName(), out Version))
			{
				return Version.Changelist;
			}
			else
			{
				return 0;
			}
		}

		public static string LoadEngineDisplayVersion(bool bIgnorePatchVersion = false)
		{
			BuildVersion? Version;
			if (BuildVersion.TryRead(BuildVersion.GetDefaultFileName(), out Version))
			{
				return String.Format("{0}.{1}.{2}", Version.MajorVersion, Version.MinorVersion, bIgnorePatchVersion ? 0 : Version.PatchVersion);
			}
			else
			{
				return "4.0.0";
			}
		}

		private int LoadBuiltFromChangelistValue()
		{
			return LoadEngineCL();
		}

		private string LoadEngineAPIVersion()
		{
			int CL = 0;

			BuildVersion? Version;
			if (BuildVersion.TryRead(BuildVersion.GetDefaultFileName(), out Version))
			{
				CL = Version.EffectiveCompatibleChangelist;
			}

			return String.Format("{0}.{1}.{2}", CL / (100 * 100), (CL / 100) % 100, CL % 100);
		}

		private string GetGameNameFromExecutablePath(string ExePath)
		{
			string ExeName = Path.GetFileName(ExePath);
			string[] ExeNameParts = ExeName.Split('-');
			string GameName = ExeNameParts[0];

			if (GameName == "EpicGamesBootstrapLauncher")
			{
				GameName = "EpicGamesLauncher";
			}
			else if (GameName == "UE5" && ProjectFile != null)
			{
				GameName = ProjectFile.GetFileNameWithoutAnyExtensions();
			}

			return GameName;
		}

		private void AddLibraryPathToRPaths(string Library, string ExeAbsolutePath, ref List<string> RPaths, ref string LinkCommand, bool bIsBuildingAppBundle, ILogger Logger)
		{
			string LibraryFullPath = Path.GetFullPath(Library);
			string LibraryDir = Path.GetDirectoryName(LibraryFullPath)!;
			string ExeDir = Path.GetDirectoryName(ExeAbsolutePath)!;

			// Only dylibs and frameworks, and only those that are outside of Engine/Binaries/Mac and Engine/Source/ThirdParty, and outside of the folder where the executable is need an additional RPATH entry
			if ((Library.EndsWith("dylib") || Library.EndsWith(".framework"))
				&& !LibraryFullPath.Contains("/Engine/Source/ThirdParty/") && LibraryDir != ExeDir && !RPaths.Contains(LibraryDir))
			{
				// macOS gatekeeper erroneously complains about not seeing the CEF3 framework in the codesigned Launcher because it's only present in one of the folders specified in RPATHs.
				// To work around this we will only add a single RPATH entry for it, for the framework stored in .app/Contents/UE/ subfolder of the packaged app bundle
				bool bCanUseMultipleRPATHs = !ExeAbsolutePath.Contains("EpicGamesLauncher-Mac-Shipping") || !Library.Contains("CEF3");

				// First, add a path relative to the executable.
				string FinalExeDir = ExeDir;
				if (bIsBuildingAppBundle)
				{
					FinalExeDir = ExeAbsolutePath + ".app/Contents/MacOS";
				}
				string RelativePath = Utils.MakePathRelativeTo(LibraryDir, FinalExeDir).Replace("\\", "/");
				

				if (bCanUseMultipleRPATHs)
				{
					LinkCommand += " -rpath \"@loader_path/" + RelativePath + "\"";

					// We are referencing plugins that need to be relative to the engine not the dylib we are building
					// To be safe leave the previous relative loader_path in place and add a relative engine executable path
					if (!bIsBuildingAppBundle && LibraryDir.Contains("/Engine/Plugins/") && ExeAbsolutePath.EndsWith("dylib"))
					{
						int EngineDirStrIndex = LibraryDir.IndexOf("Engine");
						if (EngineDirStrIndex >= 0)
						{
							LinkCommand += " -rpath \"@executable_path/../../../../../../" + LibraryDir.Substring(EngineDirStrIndex) + "\"";
						}
					}
				}

				// If building an app bundle, we also need an RPATH for use in packaged game and a separate one for staged builds
				if (bIsBuildingAppBundle)
				{
					string EngineDir = Unreal.RootDirectory.ToString();
					string? ProjectDir = ProjectFile?.Directory.FullName;

					// In packaged games dylibs are stored in Contents/UE subfolders, for example in GameName.app/Contents/UE/Engine/Binaries/ThirdParty/PhysX/Mac
					string BundleUEDir = Path.GetFullPath(ExeDir + "/../../Contents/UE");
					string? BundleLibraryDir = null;
					if (LibraryDir.StartsWith(EngineDir + "/"))
					{
						BundleLibraryDir = LibraryDir.Replace(EngineDir, BundleUEDir);
					}
					else if (ProjectDir != null && LibraryDir.StartsWith(ProjectDir + "/"))
					{
						string GameName = GetGameNameFromExecutablePath(ExeAbsolutePath);
						BundleLibraryDir = LibraryDir.Replace(ProjectDir, BundleUEDir + "/" + GameName);
					}

					if (BundleLibraryDir != null)
					{
						string BundleRelativeDir = Utils.MakePathRelativeTo(BundleLibraryDir, ExeDir).Replace("\\", "/");
						LinkCommand += " -rpath \"@loader_path/" + BundleRelativeDir + "\"";
					}
					else
					{
						Logger.LogWarning("Unexpected third party dylib location when generating RPATH entries: {LibraryFullPath}. Skipping.", LibraryFullPath);
					}

					// For staged code-based games we need additional entry if the game is not stored directly in the engine's root directory
					if (bCanUseMultipleRPATHs)
					{
						string StagedUEDir = Path.GetFullPath(ExeDir + "/../../../../../..");
						string StagedLibraryDir = LibraryDir.Replace(EngineDir, StagedUEDir);
						string StagedRelativeDir = Utils.MakePathRelativeTo(StagedLibraryDir, ExeDir).Replace("\\", "/");
						if (StagedRelativeDir != RelativePath)
						{
							LinkCommand += " -rpath \"@loader_path/" + StagedRelativeDir + "\"";
						}
					}
				}

				RPaths.Add(LibraryDir);
			}
		}

		public override FileItem[] LinkImportLibrary(LinkEnvironment LinkEnvironment, IActionGraphBuilder Graph)
		{
			// we actually create Actions for every dylib, but we never return a FileItem. Instead depend on a Link action
			// linking to the stub dylib to make thie action take place. If we returned FileItems here, UBT would link a 
			// stub lib for every dylub even if not needed by anything
			if (LinkEnvironment.bIsBuildingDLL)
			{
				LinkAllFiles(LinkEnvironment, true, Graph);
			}

			return new FileItem[] { };
		}

		public override FileItem? LinkFiles(LinkEnvironment LinkEnvironment, bool bBuildImportLibraryOnly, IActionGraphBuilder Graph)
		{
			return null;
		}

		public override FileItem[] LinkAllFiles(LinkEnvironment LinkEnvironment, bool bBuildImportLibraryOnly, IActionGraphBuilder Graph)
		{
			List<FileItem> OutputFiles = new();
			// To solve the problem with cross dependencies, for now we create a broken dylib that does not link with other engine dylibs.
			// This is fixed in later step, FixDylibDependencies. For this and to know what libraries to copy whilst creating an app bundle,
			// we gather the list of engine dylibs.
			List<string> EngineAndGameLibraries = new List<string>();

			bool bIsBuildingLibrary = LinkEnvironment.bIsBuildingLibrary || bBuildImportLibraryOnly;
			bool bIsBuildingAppBundle = !LinkEnvironment.bIsBuildingDLL && !LinkEnvironment.bIsBuildingLibrary && !LinkEnvironment.bIsBuildingConsoleApplication;
			FileReference LinkerPath = bIsBuildingLibrary ? Info.Archiver : Info.Clang;
			string LinkCommand = "";

			if (LinkEnvironment.Architectures.bIsMultiArch)
			{
				List<FileItem> PerArchOutputFiles = new();
				foreach (UnrealArch Arch in LinkEnvironment.Architectures.Architectures)
				{
					LinkEnvironment ArchEnvironment = new LinkEnvironment(LinkEnvironment, Arch);
					if (!bBuildImportLibraryOnly)
					{
						ArchEnvironment.OutputFilePaths = LinkEnvironment.OutputFilePaths.Select(x => new FileReference($"{LinkEnvironment.IntermediateDirectory}/{Path.GetFileName(x.FullName)}_{Arch}")).ToList();
					}

					PerArchOutputFiles.Add(LinkArchitectureFiles(ArchEnvironment, LinkEnvironment.OutputFilePath, bBuildImportLibraryOnly, Graph, out LinkCommand));
				}

				if (bBuildImportLibraryOnly)
				{
					return PerArchOutputFiles.ToArray();
				}

				FileItem OutputFileItem;
				if (bBuildImportLibraryOnly)
				{
					OutputFileItem = MakeStubItem(LinkEnvironment, LinkEnvironment.OutputFilePath.FullName);
				}
				else
				{
					OutputFileItem = FileItem.GetItemByFileReference(LinkEnvironment.OutputFilePath);
				}
				OutputFiles.Add(OutputFileItem);

				Action LipoAction = Graph.CreateAction(ActionType.Link);
				LipoAction.PrerequisiteItems.UnionWith(PerArchOutputFiles);
				LipoAction.ProducedItems.Add(OutputFileItem);
				LipoAction.WorkingDirectory = GetMacDevSrcRoot();
				LipoAction.CommandPath = FileReference.Combine(Info.Clang.Directory, "lipo");
				LipoAction.CommandDescription = (bBuildImportLibraryOnly ? "LipoStub" : "Lipo");
				LipoAction.CommandVersion = Info.ClangVersionString;
				LipoAction.CommandArguments = String.Join(" ", PerArchOutputFiles.Select(x => $"\"{x}\"")) + $" -create -output \"{OutputFileItem.AbsolutePath}\"";
				LipoAction.StatusDescription = Path.GetFileName(OutputFileItem.AbsolutePath);
				LipoAction.bCanExecuteRemotely = false;
			}
			else
			{
				OutputFiles.Add(LinkArchitectureFiles(LinkEnvironment, LinkEnvironment.OutputFilePath, bBuildImportLibraryOnly, Graph, out LinkCommand));
			}

			if (!DirectoryReference.Exists(LinkEnvironment.IntermediateDirectory!))
			{
				DirectoryReference.CreateDirectory(LinkEnvironment.IntermediateDirectory!);
			}

			// For non-console application, prepare a script that will create the app bundle. It'll be run by FinalizeAppBundle action
			if (bIsBuildingAppBundle && !bUseModernXcode)
			{
				FileReference FinalizeAppBundleScriptPath = FileReference.Combine(LinkEnvironment.IntermediateDirectory!, "FinalizeAppBundle.sh");
				StreamWriter FinalizeAppBundleScript = File.CreateText(FinalizeAppBundleScriptPath.FullName);
				AppendMacLine(FinalizeAppBundleScript, "#!/bin/sh");
				string BinariesPath = Path.GetDirectoryName(OutputFiles[0].AbsolutePath)!;
				BinariesPath = Path.GetDirectoryName(BinariesPath.Substring(0, BinariesPath.IndexOf(".app")))!;
				AppendMacLine(FinalizeAppBundleScript, "cd \"{0}\"", BinariesPath.Replace("$", "\\$"));

				string BundleVersion = LinkEnvironment.BundleVersion!;
				if (BundleVersion == null)
				{
					BundleVersion = LoadEngineDisplayVersion();
				}

				string ExeName = Path.GetFileName(OutputFiles[0].AbsolutePath);
				bool bIsLauncherProduct = ExeName.StartsWith("EpicGamesLauncher") || ExeName.StartsWith("EpicGamesBootstrapLauncher");
				string[] ExeNameParts = ExeName.Split('-');
				string GameName = GetGameNameFromExecutablePath(OutputFiles[0].AbsolutePath);

				// bundle identifier
				// plist replacements
				DirectoryReference? DirRef = (!String.IsNullOrEmpty(UnrealBuildTool.GetRemoteIniPath()) ? new DirectoryReference(UnrealBuildTool.GetRemoteIniPath()!) : (ProjectFile != null ? ProjectFile.Directory : null));
				ConfigHierarchy IOSIni = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirRef, UnrealTargetPlatform.IOS);

				string BundleIdentifier;
				IOSIni.GetString("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "BundleIdentifier", out BundleIdentifier!);

				string ProjectName = GameName;
				FileReference? UProjectFilePath = ProjectFile;

				if (UProjectFilePath != null)
				{
					ProjectName = UProjectFilePath.GetFileNameWithoutAnyExtensions();
				}

				AppendMacLine(FinalizeAppBundleScript, "mkdir -p \"{0}.app/Contents/MacOS\"", ExeName);
				AppendMacLine(FinalizeAppBundleScript, "mkdir -p \"{0}.app/Contents/Resources\"", ExeName);

				string IconName = "UnrealEngine";
				string EngineSourcePath = Directory.GetCurrentDirectory().Replace("$", "\\$");
				string CustomResourcesPath = "";
				string CustomBuildPath = "";
				if (UProjectFilePath == null)
				{
					string[] TargetFiles = Directory.GetFiles(Directory.GetCurrentDirectory(), GameName + ".Target.cs", SearchOption.AllDirectories);
					if (TargetFiles.Length == 1)
					{
						CustomResourcesPath = Path.GetDirectoryName(TargetFiles[0]) + "/Resources/Mac";
						CustomBuildPath = Path.GetDirectoryName(TargetFiles[0]) + "../Build/Mac";
					}
					else
					{
						Logger.LogWarning("Found {NumFiles} Target.cs files for {GameName} in alldir search of directory {Dir}", TargetFiles.Length, GameName, Directory.GetCurrentDirectory());
					}
				}
				else
				{
					string ResourceParentFolderName = bIsLauncherProduct ? "Application" : GameName;
					CustomResourcesPath = Path.GetDirectoryName(UProjectFilePath.FullName) + "/Source/" + ResourceParentFolderName + "/Resources/Mac";
					if (!Directory.Exists(CustomResourcesPath))
					{
						CustomResourcesPath = Path.GetDirectoryName(UProjectFilePath.FullName) + "/Source/" + ProjectName + "/Resources/Mac";
					}
					CustomBuildPath = Path.GetDirectoryName(UProjectFilePath.FullName) + "/Build/Mac";
				}

				bool bBuildingEditor = GameName.EndsWith("Editor");

				// Copy resources
				string DefaultIcon = EngineSourcePath + "/Runtime/Launch/Resources/Mac/" + IconName + ".icns";
				string CustomIcon = "";
				if (bBuildingEditor)
				{
					CustomIcon = DefaultIcon;
				}
				else
				{
					CustomIcon = CustomBuildPath + "/Application.icns";
					if (!File.Exists(CustomIcon))
					{
						CustomIcon = CustomResourcesPath + "/" + GameName + ".icns";
						if (!File.Exists(CustomIcon))
						{
							CustomIcon = DefaultIcon;
						}
					}
				}
				AppendMacLine(FinalizeAppBundleScript, FormatCopyCommand(CustomIcon, String.Format("{0}.app/Contents/Resources/{1}.icns", ExeName, GameName)));

				if (ExeName.StartsWith("UnrealEditor"))
				{
					AppendMacLine(FinalizeAppBundleScript, FormatCopyCommand(String.Format("{0}/Runtime/Launch/Resources/Mac/UProject.icns", EngineSourcePath), String.Format("{0}.app/Contents/Resources/UProject.icns", ExeName)));
				}

				string InfoPlistFile = CustomResourcesPath + (bBuildingEditor ? "/Info-Editor.plist" : "/Info.plist");
				if (!File.Exists(InfoPlistFile))
				{
					InfoPlistFile = EngineSourcePath + "/Runtime/Launch/Resources/Mac/" + (bBuildingEditor ? "Info-Editor.plist" : "Info.plist");
				}

				string TempInfoPlist = "$TMPDIR/TempInfo.plist";
				AppendMacLine(FinalizeAppBundleScript, FormatCopyCommand(InfoPlistFile, TempInfoPlist));

				// Fix contents of Info.plist
				AppendMacLine(FinalizeAppBundleScript, "/usr/bin/sed -i \"\" -e \"s/\\${0}/{1}/g\" \"{2}\"", "{EXECUTABLE_NAME}", ExeName, TempInfoPlist);
				AppendMacLine(FinalizeAppBundleScript, "/usr/bin/sed -i \"\" -e \"s/\\${0}/{1}/g\" \"{2}\"", "{APP_NAME}", bBuildingEditor ? ("com.epicgames." + GameName) : (BundleIdentifier.Replace("[PROJECT_NAME]", GameName).Replace("_", "")), TempInfoPlist);
				AppendMacLine(FinalizeAppBundleScript, "/usr/bin/sed -i \"\" -e \"s/\\${0}/{1}/g\" \"{2}\"", "{MACOSX_DEPLOYMENT_TARGET}", Settings.MinMacDeploymentVersion(Target!.Type), TempInfoPlist);
				AppendMacLine(FinalizeAppBundleScript, "/usr/bin/sed -i \"\" -e \"s/\\${0}/{1}/g\" \"{2}\"", "{ICON_NAME}", GameName, TempInfoPlist);
				AppendMacLine(FinalizeAppBundleScript, "/usr/bin/sed -i \"\" -e \"s/\\${0}/{1}/g\" \"{2}\"", "{BUNDLE_VERSION}", BundleVersion, TempInfoPlist);

				// Copy it into place
				AppendMacLine(FinalizeAppBundleScript, FormatCopyCommand(TempInfoPlist, String.Format("{0}.app/Contents/Info.plist", ExeName)));
				AppendMacLine(FinalizeAppBundleScript, "chmod 644 \"{0}.app/Contents/Info.plist\"", ExeName);

				// Also copy it to where Xcode will look for it, now that Xcode 14 requires we have one set up - it can't point into the .app because that
				// is where it will copy to, so it will error with reading and writing to the same location
				string IntermediateDirectory = (ProjectFile == null ? Unreal.EngineDirectory : ProjectFile.Directory) + "/Intermediate/Mac";
				string XcodeInputPListFile = IntermediateDirectory + "/" + ExeName + "-Info.plist";
				AppendMacLine(FinalizeAppBundleScript, "mkdir -p \"{0}\"", IntermediateDirectory);
				AppendMacLine(FinalizeAppBundleScript, FormatCopyCommand(TempInfoPlist, XcodeInputPListFile));
				AppendMacLine(FinalizeAppBundleScript, "chmod 644 \"{0}\"", XcodeInputPListFile);

				// Generate PkgInfo file
				string TempPkgInfo = "$TMPDIR/TempPkgInfo";
				AppendMacLine(FinalizeAppBundleScript, "echo 'echo -n \"APPL????\"' | bash > \"{0}\"", TempPkgInfo);
				AppendMacLine(FinalizeAppBundleScript, FormatCopyCommand(TempPkgInfo, String.Format("{0}.app/Contents/PkgInfo", ExeName)));

				// Make sure OS X knows the bundle was updated
				AppendMacLine(FinalizeAppBundleScript, "touch -c \"{0}.app\"", ExeName);

				// codesign with ad-hoc signature for when building outside of Xcode the there will be at least some signature
				// (and it can be a BuildProduct down in ModifyBuildProducts)
				AppendMacLine(FinalizeAppBundleScript, "codesign -f -s - \"{0}.app\"", ExeName);
				AppendMacLine(FinalizeAppBundleScript, "codesign -f -s \"Developer ID Application\" \"{0}.app\" 2> /dev/null", ExeName);
				AppendMacLine(FinalizeAppBundleScript, "echo done > /dev/null");

				FinalizeAppBundleScript.Close();
			}

			return OutputFiles.ToArray();
		}

		private FileItem MakeStubItem(LinkEnvironment LinkEnvironment, string DylibPath)
		{
			FileReference DylibPathRef = new FileReference(DylibPath);
			bool bIsEngineDylib = DylibPathRef.IsUnderDirectory(Unreal.EngineDirectory);

			string IntermediateDirectory = (ProjectFile == null || LinkEnvironment.OutputFilePaths.All(x => x.IsUnderDirectory(Unreal.EngineDirectory)) || bIsEngineDylib ? Unreal.EngineDirectory : ProjectFile.Directory) + "/Intermediate/Mac";
			return FileItem.GetItemByPath(Path.Combine(IntermediateDirectory, "Stubs", LinkEnvironment.Architecture.ToString(), LinkEnvironment.Configuration.ToString(), Path.GetFileName(DylibPath)));
		}

		private FileItem LinkArchitectureFiles(LinkEnvironment LinkEnvironment, FileReference MultiArchOutputFile, bool bBuildImportLibraryOnly, IActionGraphBuilder Graph, out string LinkCommand)
		{
			List<FileItem> InputFiles = LinkEnvironment.InputFiles;

			// with precompiled builds, we end up trying to link the arm64 and x64 versions of the engine .o files. Remove the wrong one
			// until we can separate out the .o files by architecture
			string ArchToRemove = LinkEnvironment.Architecture == UnrealArch.Arm64 ? "x64" : "arm64";
			InputFiles.RemoveAll(x => x.Location.ContainsName(ArchToRemove, 0));

			// Create an action that invokes the linker.
			Action LinkAction = Graph.CreateAction(ActionType.Link);

			FileReference LinkerPath = LinkEnvironment.bIsBuildingLibrary ? Info.Archiver : Info.Clang;

			LinkAction.WorkingDirectory = GetMacDevSrcRoot();
			LinkAction.CommandPath = LinkerPath;
			string Arch = UnrealArchitectureConfig.ForPlatform(UnrealTargetPlatform.Mac).ConvertToReadableArchitecture(LinkEnvironment.Architecture);
			LinkAction.CommandDescription = (bBuildImportLibraryOnly ? "LinkStub" : "Link") + $" [{Arch}]";
			LinkAction.CommandVersion = Info.ClangVersionString;
			LinkAction.bProducesImportLibrary = bBuildImportLibraryOnly || LinkEnvironment.bIsBuildingDLL;

			string EngineAPIVersion = LoadEngineAPIVersion();
			string EngineDisplayVersion = LoadEngineDisplayVersion(true);
			string VersionArg = LinkEnvironment.bIsBuildingDLL ? " -current_version " + EngineAPIVersion + " -compatibility_version " + EngineDisplayVersion : "";

			List<string> LinkArguments = new();
			if (LinkEnvironment.bIsBuildingLibrary)
			{
				GetArchiveArguments_Global(LinkEnvironment, LinkArguments);
			}
			else
			{
				GetLinkArguments_Global(LinkEnvironment, LinkArguments);
			}

			LinkCommand = VersionArg + " " + String.Join(' ', LinkArguments);

			// Tell the action that we're building an import library here and it should conditionally be
			// ignored as a prerequisite for other actions
			LinkAction.bProducesImportLibrary = bBuildImportLibraryOnly || LinkEnvironment.bIsBuildingDLL;

			// Add the output file as a production of the link action.
			FileItem OutputFile;
			if (bBuildImportLibraryOnly)
			{
				OutputFile = MakeStubItem(LinkEnvironment, LinkEnvironment.OutputFilePath.FullName);
			}
			else
			{
				OutputFile = FileItem.GetItemByFileReference(LinkEnvironment.OutputFilePath);
			}

			string DylibsPath = "@rpath";

			string AbsolutePath = MultiArchOutputFile.FullName.Replace("\\", "/");
			if (!LinkEnvironment.bIsBuildingLibrary)
			{
				LinkCommand += " -rpath @loader_path/ -rpath @executable_path/";
			}

			bool bIsBuildingAppBundle = !LinkEnvironment.bIsBuildingDLL && !LinkEnvironment.bIsBuildingLibrary && !LinkEnvironment.bIsBuildingConsoleApplication;
			if (bIsBuildingAppBundle)
			{
				LinkCommand += " -rpath @executable_path/../../../";
			}

			List<string> RPaths = new List<string>();

			// static libs and stub dylibs don't need to look other libs
			if (bBuildImportLibraryOnly)
			{
				LinkCommand += " -undefined dynamic_lookup";
				LinkCommand += " -Wl,-no_fixup_chains";
			}
			else if (!LinkEnvironment.bIsBuildingLibrary)
			{
				// Add the additional libraries to the argument list.
				IEnumerable<string> AdditionalLibraries = Enumerable.Concat(LinkEnvironment.SystemLibraries, LinkEnvironment.Libraries.Select(x => x.FullName));
				foreach (string AdditionalLibrary in AdditionalLibraries)
				{
					if (!String.IsNullOrEmpty(Path.GetDirectoryName(AdditionalLibrary)) &&
							 (Path.GetDirectoryName(AdditionalLibrary)!.Contains("Binaries/Mac") ||
							 Path.GetDirectoryName(AdditionalLibrary)!.Contains("Binaries\\Mac")))
					{
						// It's an engine or game dylib. Save it for later
						FileItem LinkedLib;
						if (LinkEnvironment.bIsCrossReferenced)
						{
							LinkedLib = MakeStubItem(LinkEnvironment, AdditionalLibrary);
						}
						else
						{
							LinkedLib = FileItem.GetItemByPath(AdditionalLibrary);
						}

						LinkAction.PrerequisiteItems.Add(LinkedLib);
						LinkCommand += $" \"{LinkedLib.FullName}\"";
					}
					else if (AdditionalLibrary.Contains(".framework/"))
					{
						LinkCommand += String.Format(" \"{0}\"", AdditionalLibrary);
					}
					else if (String.IsNullOrEmpty(Path.GetDirectoryName(AdditionalLibrary)) && String.IsNullOrEmpty(Path.GetExtension(AdditionalLibrary)))
					{
						LinkCommand += String.Format(" -l\"{0}\"", AdditionalLibrary);
					}
					else
					{
						LinkCommand += String.Format(" \"{0}\"", Path.GetFullPath(AdditionalLibrary));
					}

					AddLibraryPathToRPaths(AdditionalLibrary, AbsolutePath, ref RPaths, ref LinkCommand, bIsBuildingAppBundle, Logger);
				}

				foreach (string AdditionalLibrary in LinkEnvironment.DelayLoadDLLs)
				{
					LinkCommand += String.Format(" -weak_library \"{0}\"", Path.GetFullPath(AdditionalLibrary));

					AddLibraryPathToRPaths(AdditionalLibrary, AbsolutePath, ref RPaths, ref LinkCommand, bIsBuildingAppBundle, Logger);
				}

				// Add frameworks
				Dictionary<string, bool> AllFrameworks = new Dictionary<string, bool>();
				foreach (string Framework in LinkEnvironment.Frameworks)
				{
					if (!AllFrameworks.ContainsKey(Framework))
					{
						AllFrameworks.Add(Framework, false);
					}
				}
				foreach (UEBuildFramework Framework in LinkEnvironment.AdditionalFrameworks)
				{
					if (Framework.ZipFile != null)
					{
						FileItem ExtractedTokenFile = ExtractFramework(Framework, Graph, Logger);
						LinkAction.PrerequisiteItems.Add(ExtractedTokenFile);
					}

					if (Framework.bLinkFramework && !AllFrameworks.ContainsKey(Framework.Name))
					{
						AllFrameworks.Add(Framework.Name, false);
					}
				}
				foreach (string Framework in LinkEnvironment.WeakFrameworks)
				{
					if (!AllFrameworks.ContainsKey(Framework))
					{
						AllFrameworks.Add(Framework, true);
					}
				}

				foreach (KeyValuePair<string, bool> Framework in AllFrameworks)
				{
					LinkCommand += AddFrameworkToLinkCommand(Framework.Key, Framework.Value ? "-weak_framework" : "-framework");
					AddLibraryPathToRPaths(Framework.Key, AbsolutePath, ref RPaths, ref LinkCommand, bIsBuildingAppBundle, Logger);
				}

				// Write the MAP file to the output directory.
				if (LinkEnvironment.bCreateMapFile)
				{
					string MapFileBaseName = OutputFile.AbsolutePath;

					int AppIdx = MapFileBaseName.IndexOf(".app/Contents/MacOS");
					if (AppIdx != -1)
					{
						MapFileBaseName = MapFileBaseName.Substring(0, AppIdx);
					}

					FileReference MapFilePath = new FileReference(MapFileBaseName + ".map");
					FileItem MapFile = FileItem.GetItemByFileReference(MapFilePath);
					LinkCommand += String.Format(" -Wl,-map,\"{0}\"", MapFilePath);
					LinkAction.ProducedItems.Add(MapFile);
				}
			}

			if (LinkEnvironment.bIsBuildingDLL)
			{
				// Add the output file to the command-line.
				string? InstallName = LinkEnvironment.InstallName;
				if (InstallName == null)
				{
					InstallName = String.Format("{0}/{1}", DylibsPath, Path.GetFileName(OutputFile.AbsolutePath).Replace($".dylib_{LinkEnvironment.Architecture}", ".dylib"));
				}
				LinkCommand += String.Format(" -install_name \"{0}\"", InstallName);
			}

			List<string> InputFileNames = new List<string>();
			foreach (FileItem InputFile in InputFiles)
			{
				InputFileNames.Add(String.Format("\"{0}\"", NormalizeCommandLinePath(InputFile)));
				LinkAction.PrerequisiteItems.Add(InputFile);
			}

			foreach (string Filename in InputFileNames)
			{
				LinkCommand += " " + Filename;
			}

			// Add the output file to the command-line.
			LinkCommand += String.Format(" -o \"{0}\"", OutputFile.AbsolutePath);

			// Add the additional arguments specified by the environment.
			LinkCommand += LinkEnvironment.AdditionalArguments;

			LinkAction.CommandArguments = LinkCommand;

			// Only execute linking on the local Mac.
			LinkAction.bCanExecuteRemotely = false;

			LinkAction.StatusDescription = Path.GetFileName(OutputFile.AbsolutePath);

			LinkAction.ProducedItems.Add(OutputFile);

			// Delete all items we produce
			LinkAction.DeleteItems.UnionWith(LinkAction.ProducedItems);

			return OutputFile;
		}

		static string FormatCopyCommand(string SourceFile, string TargetFile)
		{
			return String.Format("rsync --checksum \"{0}\" \"{1}\"", SourceFile, TargetFile);
		}

		/// <summary>
		/// Generates debug info for a given executable
		/// </summary>
		/// <param name="MachOBinary">FileItem describing the executable or dylib to generate debug info for</param>
		/// <param name="LinkEnvironment"></param>
		/// <param name="Graph">List of actions to be executed. Additional actions will be added to this list.</param>
		/// <param name="Logger">Logger for output</param>
		public FileItem GenerateDebugInfo(FileItem MachOBinary, LinkEnvironment LinkEnvironment, IActionGraphBuilder Graph, ILogger Logger)
		{
			string BinaryPath = MachOBinary.AbsolutePath;
			if (BinaryPath.Contains(".app"))
			{
				while (BinaryPath.Contains(".app"))
				{
					BinaryPath = Path.GetDirectoryName(BinaryPath)!;
				}
				BinaryPath = Path.Combine(BinaryPath, Path.GetFileName(Path.ChangeExtension(MachOBinary.AbsolutePath, ".dSYM")));
			}
			else
			{
				BinaryPath = Path.ChangeExtension(BinaryPath, ".dSYM");
			}

			FileItem OutputFile = FileItem.GetItemByPath(BinaryPath);

			// Delete on the local machine
			if (Directory.Exists(OutputFile.AbsolutePath))
			{
				Directory.Delete(OutputFile.AbsolutePath, true);
			}

			// Make the compile action
			Action GenDebugAction = Graph.CreateAction(ActionType.GenerateDebugInfo);
			GenDebugAction.WorkingDirectory = GetMacDevSrcRoot();
			GenDebugAction.CommandPath = BuildHostPlatform.Current.Shell;

			// Deletes ay existing file on the building machine. Also, waits 30 seconds, if needed, for the input file to be created in an attempt to work around
			// a problem where dsymutil would exit with an error saying the input file did not exist.
			// Note that the source and dest are switched from a copy command
			string ExtraOptions = "";
			string DsymutilPath = GetDsymutilPath(Logger, out ExtraOptions, bIsForLTOBuild: false);
			string UniversalDsymutilScriptPath = FileReference.Combine(Unreal.EngineDirectory, "Build/BatchFiles/Mac/GenerateUniversalDSYM.sh").FullName;

			string ArgumentString = "-c \"";
			ArgumentString += String.Format("for i in {{1..30}}; ");
			ArgumentString += String.Format("do if [ -f \\\"{0}\\\" ] ; ", MachOBinary.AbsolutePath);
			ArgumentString += String.Format("then ");
			ArgumentString += String.Format("break; ");
			ArgumentString += String.Format("else ");
			ArgumentString += String.Format("sleep 1; ");
			ArgumentString += String.Format("fi; ");
			ArgumentString += String.Format("done; ");

			ArgumentString += String.Format("if [ ! -f \\\"{1}\\\" ] || [ \\\"{0}\\\" -nt \\\"{1}\\\" ] ; ", MachOBinary.AbsolutePath, OutputFile.AbsolutePath);
			ArgumentString += String.Format("then ");
			ArgumentString += String.Format("rm -rf \\\"{0}\\\"; ", OutputFile.AbsolutePath);
			// use the new script for monolthic (ie large) targets
			if (LinkEnvironment.LinkType == TargetLinkType.Monolithic)
			{
				ArgumentString += String.Format(" \\\"{0}\\\" \\\"{1}\\\" \\\"{2}\\\"; ",
					UniversalDsymutilScriptPath,
					MachOBinary.AbsolutePath,
					OutputFile.AbsolutePath);
			}
			else
			{
				ArgumentString += String.Format(" \\\"{0}\\\" {3} -f \\\"{1}\\\" -o \\\"{2}\\\"; ",
					DsymutilPath,
					MachOBinary.AbsolutePath,
					OutputFile.AbsolutePath,
					ExtraOptions);
			}
			ArgumentString += String.Format("fi; ");
			ArgumentString += "\"";

			GenDebugAction.CommandArguments = ArgumentString;

			GenDebugAction.PrerequisiteItems.Add(MachOBinary);
			GenDebugAction.ProducedItems.Add(OutputFile);
			GenDebugAction.CommandDescription = "";
			GenDebugAction.StatusDescription = "Generating " + Path.GetFileName(BinaryPath);
			GenDebugAction.bCanExecuteRemotely = false;

			return OutputFile;
		}

		/// <summary>
		/// Creates app bundle for a given executable
		/// </summary>
		/// <param name="Target"></param>
		/// <param name="LinkEnvironment"></param>
		/// <param name="Executable">FileItem describing the executable to generate app bundle for</param>
		/// <param name="Graph">List of actions to be executed. Additional actions will be added to this list.</param>
		FileItem FinalizeAppBundle(ReadOnlyTargetRules Target, LinkEnvironment LinkEnvironment, FileItem Executable, IActionGraphBuilder Graph)
		{
			// Make a file item for the source and destination files
			string FullDestPath = Executable.AbsolutePath.Substring(0, Executable.AbsolutePath.IndexOf(".app") + 4);
			FileItem DestFile = FileItem.GetItemByPath(FullDestPath);

			// Make the compile action
			Action FinalizeAppBundleAction = Graph.CreateAction(ActionType.CreateAppBundle);
			FinalizeAppBundleAction.WorkingDirectory = GetMacDevSrcRoot(); // Path.GetFullPath(".");
			FinalizeAppBundleAction.CommandPath = BuildHostPlatform.Current.Shell;
			FinalizeAppBundleAction.CommandDescription = "";

			// make path to the script
			FileItem BundleScript = FileItem.GetItemByFileReference(FileReference.Combine(LinkEnvironment.IntermediateDirectory!, "FinalizeAppBundle.sh"));

			FinalizeAppBundleAction.CommandArguments = "\"" + BundleScript.AbsolutePath + "\"";
			FinalizeAppBundleAction.PrerequisiteItems.Add(Executable);
			FinalizeAppBundleAction.ProducedItems.Add(DestFile);
			FinalizeAppBundleAction.StatusDescription = String.Format("Finalizing app bundle: {0}.app", Path.GetFileName(Executable.AbsolutePath));
			FinalizeAppBundleAction.bCanExecuteRemotely = false;

			return DestFile;
		}

		FileItem CopyBundleResource(UEBuildBundleResource Resource, FileItem Executable, DirectoryReference BundleDirectory, IActionGraphBuilder Graph)
		{
			Action CopyAction = Graph.CreateAction(ActionType.CreateAppBundle);
			CopyAction.WorkingDirectory = GetMacDevSrcRoot(); // Path.GetFullPath(".");
			CopyAction.CommandPath = BuildHostPlatform.Current.Shell;
			CopyAction.CommandDescription = "";

			string BundlePath = BundleDirectory.FullName;
			string SourcePath = Path.Combine(Path.GetFullPath("."), Resource.ResourcePath!);
			string TargetPath = Path.Combine(BundlePath, "Contents", Resource.BundleContentsSubdir!, Path.GetFileName(Resource.ResourcePath)!);

			FileItem TargetItem = FileItem.GetItemByPath(TargetPath);

			CopyAction.CommandArguments = String.Format("-c \"cp -f -R \\\"{0}\\\" \\\"{1}\\\"; touch -c \\\"{2}\\\"\"", SourcePath, Path.GetDirectoryName(TargetPath)!.Replace('\\', '/') + "/", TargetPath.Replace('\\', '/'));
			CopyAction.PrerequisiteItems.Add(Executable);
			CopyAction.ProducedItems.Add(TargetItem);
			CopyAction.bShouldOutputStatusDescription = Resource.bShouldLog;
			CopyAction.StatusDescription = String.Format("Copying {0} to app bundle", Path.GetFileName(Resource.ResourcePath));
			CopyAction.bCanExecuteRemotely = false;

			return TargetItem;
		}

		private static Dictionary<ReadOnlyTargetRules, DirectoryReference> BundleContentsDirectories = new();

		public override void ModifyBuildProducts(ReadOnlyTargetRules Target, UEBuildBinary Binary, IEnumerable<string> Libraries, IEnumerable<UEBuildBundleResource> BundleResources, Dictionary<FileReference, BuildProductType> BuildProducts)
		{
			if (Target.bUsePDBFiles == true)
			{
				KeyValuePair<FileReference, BuildProductType>[] BuildProductsArray = BuildProducts.ToArray();

				foreach (KeyValuePair<FileReference, BuildProductType> BuildProductPair in BuildProductsArray)
				{
					string[] DebugExtensions = new string[] { };
					switch (BuildProductPair.Value)
					{
						case BuildProductType.Executable:
							DebugExtensions = UEBuildPlatform.GetBuildPlatform(Target.Platform).GetDebugInfoExtensions(Target, UEBuildBinaryType.Executable);
							break;
						case BuildProductType.DynamicLibrary:
							DebugExtensions = UEBuildPlatform.GetBuildPlatform(Target.Platform).GetDebugInfoExtensions(Target, UEBuildBinaryType.DynamicLinkLibrary);
							break;
					}
					string? DSYMExtension = Array.Find(DebugExtensions, element => element == ".dSYM");
					if (!String.IsNullOrEmpty(DSYMExtension))
					{
						string BinaryPath = BuildProductPair.Key.FullName;
						if (BinaryPath.Contains(".app"))
						{
							while (BinaryPath.Contains(".app"))
							{
								BinaryPath = Path.GetDirectoryName(BinaryPath)!;
							}
							BinaryPath = Path.Combine(BinaryPath, BuildProductPair.Key.GetFileName());
							BinaryPath = Path.ChangeExtension(BinaryPath, DSYMExtension);
							FileReference Ref = new FileReference(BinaryPath);
							BuildProducts[Ref] = BuildProductType.SymbolFile;
						}
					}
					else if (BuildProductPair.Value == BuildProductType.SymbolFile && BuildProductPair.Key.FullName.Contains(".app"))
					{
						BuildProducts.Remove(BuildProductPair.Key);
					}
					if (BuildProductPair.Value == BuildProductType.DynamicLibrary && Target.bCreateMapFile)
					{
						BuildProducts.Add(new FileReference(BuildProductPair.Key.FullName + ".map"), BuildProductType.MapFile);
					}
				}
			}

			if (Target.bIsBuildingConsoleApplication)
			{
				return;
			}

			if (!BundleContentsDirectories.ContainsKey(Target) && Binary.Type == UEBuildBinaryType.Executable)
			{
				// For Mac binary executables, we may build it outside of app, but expect it to be inside of .app after modern Xcode does its thing
				// We still keep the executable outside of app as RequiredResource, so that Horde will copy in-between agents
				FileReference FinalBinaryPath = Binary.OutputFilePath;
				if (!FinalBinaryPath.Directory.FullName.EndsWith(".app/Contents/MacOS", StringComparison.OrdinalIgnoreCase))
				{
					FinalBinaryPath = FileReference.Combine(FinalBinaryPath.Directory, FinalBinaryPath.GetFileName() + ".app", "Contents", "MacOS", FinalBinaryPath.GetFileName());
					BuildProducts[Binary.OutputFilePath] = BuildProductType.RequiredResource;
					BuildProducts.Add(FinalBinaryPath, BuildProductType.Executable);
				}
				BundleContentsDirectories.Add(Target, FinalBinaryPath.Directory.ParentDirectory!);
			}
			DirectoryReference? BundleContentsDirectory = BundleContentsDirectories.GetValueOrDefault(Target);

			// We need to know what third party dylibs would be copied to the bundle
			if (Binary.Type != UEBuildBinaryType.StaticLibrary)
			{
				foreach (UEBuildBundleResource Resource in BundleResources)
				{
					if (Directory.Exists(Resource.ResourcePath))
					{
						foreach (string ResourceFile in Directory.GetFiles(Resource.ResourcePath, "*", SearchOption.AllDirectories))
						{
							BuildProducts.Add(FileReference.Combine(BundleContentsDirectory!, Resource.BundleContentsSubdir!, ResourceFile.Substring(Path.GetDirectoryName(Resource.ResourcePath)!.Length + 1)), BuildProductType.RequiredResource);
						}
					}
					else if (BundleContentsDirectory != null)
					{
						BuildProducts.Add(FileReference.Combine(BundleContentsDirectory, Resource.BundleContentsSubdir!, Path.GetFileName(Resource.ResourcePath)!), BuildProductType.RequiredResource);
					}
				}
			}

			if (Binary.Type == UEBuildBinaryType.Executable)
			{
				// And we also need all the resources
				BuildProducts.Add(FileReference.Combine(BundleContentsDirectory!, "Info.plist"), BuildProductType.RequiredResource);
				BuildProducts.Add(FileReference.Combine(BundleContentsDirectory!, "PkgInfo"), BuildProductType.RequiredResource);

				// when we codesign, we need to copy the signature around on build machines, etc. this will put the CodeResources (that were created by post-build signing)
				// is in the .target receipt file
				BuildProducts.Add(FileReference.Combine(BundleContentsDirectory!, "_CodeSignature", "CodeResources"), BuildProductType.RequiredResource);

				// modern xcode doesn't use the bootstrap launcher because it can make full .app with staged data inside it
				if (!bUseModernXcode)
				{
					if (Target.Type == TargetType.Editor)
					{
						BuildProducts.Add(FileReference.Combine(BundleContentsDirectory!, "Resources/UnrealEditor.icns"), BuildProductType.RequiredResource);
						BuildProducts.Add(FileReference.Combine(BundleContentsDirectory!, "Resources/UProject.icns"), BuildProductType.RequiredResource);
					}
					else
					{
						string IconName = Target.Name;
						if (IconName == "EpicGamesBootstrapLauncher")
						{
							IconName = "EpicGamesLauncher";
						}
						BuildProducts.Add(FileReference.Combine(BundleContentsDirectory!, "Resources/" + IconName + ".icns"), BuildProductType.RequiredResource);
					}
				}
			}
		}

		private List<FileItem> DebugInfoFiles = new List<FileItem>();

		public override void FinalizeOutput(ReadOnlyTargetRules Target, TargetMakefileBuilder MakefileBuilder)
		{
			base.FinalizeOutput(Target, MakefileBuilder);

			TargetMakefile Makefile = MakefileBuilder.Makefile;

			// Re-add any .dSYM files that may have been stripped out.
			List<string> OutputFiles = Makefile.OutputItems.Select(Item => Path.ChangeExtension(Item.FullName, ".dSYM")).Distinct().ToList();
			foreach (FileItem DebugItem in DebugInfoFiles)
			{
				if (OutputFiles.Any(Item => String.Equals(Item, DebugItem.FullName, StringComparison.InvariantCultureIgnoreCase)))
				{
					Makefile.OutputItems.Add(DebugItem);
				}
			}
		}

		public override ICollection<FileItem> PostBuild(ReadOnlyTargetRules Target, FileItem Executable, LinkEnvironment BinaryLinkEnvironment, IActionGraphBuilder Graph)
		{
			ICollection<FileItem> OutputFiles = base.PostBuild(Target, Executable, BinaryLinkEnvironment, Graph);

			if (BinaryLinkEnvironment.bIsBuildingLibrary)
			{
				return OutputFiles;
			}

			// modern handle bundles via xcode
			if (!bUseModernXcode)
			{
				if (BinaryLinkEnvironment.BundleDirectory != null)
				{
					foreach (UEBuildBundleResource Resource in BinaryLinkEnvironment.AdditionalBundleResources)
					{
						OutputFiles.Add(CopyBundleResource(Resource, Executable, BinaryLinkEnvironment.BundleDirectory, Graph));
					}
				}
			}

			// For Mac, generate the dSYM file if the config file is set to do so
			if (BinaryLinkEnvironment.bUsePDBFiles == true)
			{
				DebugInfoFiles.Add(GenerateDebugInfo(Executable, BinaryLinkEnvironment, Graph, Logger));
			}

			if ((BinaryLinkEnvironment.bIsBuildingDLL && (Options & ClangToolChainOptions.OutputDylib) == 0) || (BinaryLinkEnvironment.bIsBuildingConsoleApplication && Executable.Name.EndsWith("-Cmd")))
			{
				return OutputFiles;
			}

			bool bIsBuildingAppBundle = !BinaryLinkEnvironment.bIsBuildingDLL && !BinaryLinkEnvironment.bIsBuildingLibrary && !BinaryLinkEnvironment.bIsBuildingConsoleApplication;
			if (bIsBuildingAppBundle)
			{
				if (!bUseModernXcode)
				{
					OutputFiles.Add(FinalizeAppBundle(Target, BinaryLinkEnvironment, Executable, Graph));
				}
			}

			return OutputFiles;
		}

		public void StripSymbols(FileReference SourceFile, FileReference TargetFile)
		{
			StripSymbolsWithXcode(SourceFile, TargetFile, Settings.ToolchainDir);
		}
	};
}
