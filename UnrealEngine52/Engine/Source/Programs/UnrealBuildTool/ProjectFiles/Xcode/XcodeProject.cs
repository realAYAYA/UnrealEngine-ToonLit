﻿// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Xml;
using System.Xml.XPath;
using System.Xml.Linq;
using System.Linq;
using System.Text;
using EpicGames.Core;
using UnrealBuildBase;
using Microsoft.Extensions.Logging;
using System.Text.RegularExpressions;
using System.Diagnostics.CodeAnalysis;


/*****

Here's how this works:

  * An XcodeProjectFile (subclass of generic ProjectFile class) is created, along with it - UnrealData and XcodeFileCollection objects are made
  * High level code calls AddModule() which this code will use to cache information about the Modules in the project (including build settings, etc)
    * These are used to determine what source files can be indexed together (we use native xcode code compilation for indexing, so we make compiling succesful for best index)
    * A few #defines are removed or modified (FOO_API, etc) which would otherwise make every module a separate target
  * High level code then calls WriteProjectFile() which is the meat of all this
  * This code then creates a hierarchy/reference-chain of xcode project nodes (an xcode project is basically a series of Guid/object pairs that reference each other)
  * Then each node writes itself into the project file that is saved to disk

.xcconfig files:
  * We also now use Xcconfig files for all of the build settings, instead of jamming them into the xcode project file itself
  * This makes it easier to see the various settings (you can also see them in the Xcode UI as before, but now with more read-only columns - they must be set via editing the file)
  * The files are in the Xcode file browsing pane, in the an Xcconfigs folder under each project
  * The files are:
    * _Project - settings apply to all targets in the projectapplies to all targets), one for each configuration (Debug, Development Editor, etc) which applies to all targets
    * _Debug/_DevelopmentEditor, etc - applies to all targets when building in that configuration
    * _Run - applies to the native run project, which does no compiling
    * _Index/SubIndex0, etc - applies when Indexing, which uses native Xcode compiling (these will include #defines from UBT, etc)
  * There is currently no _Build xcconfig file since it's a makefile project and has no build setting needs


Known issues:
  * No PBXFrameworksBuildPhase nodes are made
 
Future ideas:
  * I have started working on a Template project that we merge Build/Index targets into, which will allow a licensee to setup codesigning, add extensions, etc. 
  * Always make a final build xcodeproj from UBT for handling the codesigning/extensions/frameworks
  * Allow for non-conflicting #defines to share SubIndex targets, hopefully will greatly reduce the sub targets in UE5

**/

namespace UnrealBuildTool.XcodeProjectXcconfig
{
	static class StringBuilderExtensions
	{
		public static void WriteLine(this StringBuilder SB, string Line = "")
		{
			SB.Append(Line);
			SB.Append(ProjectFileGenerator.NewLine);
		}
		public static void WriteLine(this StringBuilder SB, int Indent, string Line = "")
		{
			SB.Append(new String('\t', Indent));
			SB.Append(Line);
			SB.Append(ProjectFileGenerator.NewLine);
		}
	}


	class UnrealBuildConfig
	{
		public UnrealBuildConfig(string InDisplayName, string InBuildTarget, FileReference? InMacExecutablePath, FileReference? InIOSExecutablePath, FileReference? InTVOSExecutablePath,
			ProjectTarget? InProjectTarget, UnrealTargetConfiguration InBuildConfig)
		{
			DisplayName = InDisplayName;
			MacExecutablePath = InMacExecutablePath;
			IOSExecutablePath = InIOSExecutablePath;
			TVOSExecutablePath = InTVOSExecutablePath;
			BuildTarget = InBuildTarget;
			ProjectTarget = InProjectTarget;
			BuildConfig = InBuildConfig;
		}

		public string DisplayName;
		public FileReference? MacExecutablePath;
		public FileReference? IOSExecutablePath;
		public FileReference? TVOSExecutablePath;
		public string BuildTarget;
		public ProjectTarget? ProjectTarget;
		public UnrealTargetConfiguration BuildConfig;

		public bool bSupportsMac => Supports(UnrealTargetPlatform.Mac);
		public bool bSupportsIOS => Supports(UnrealTargetPlatform.IOS);
		public bool bSupportsTVOS => Supports(UnrealTargetPlatform.TVOS);
		public bool Supports(UnrealTargetPlatform? Platform)
		{
			return UnrealData.Supports(Platform) && (ProjectTarget == null || Platform == null || ProjectTarget.SupportedPlatforms.Contains((UnrealTargetPlatform)Platform));
		}
	};

	class UnrealBatchedFiles
	{
		// build settings that cause uniqueness
		public string Definitions = "";
		// @todo can we actually use this effectively with indexing other than fotced include?
		public FileReference? PCHFile = null;
		public bool bEnableRTTI = false;

		// union of settings for all modules
		public HashSet<string> AllDefines = new() { "__INTELLISENSE__", "MONOLITHIC_BUILD=1" };
		public HashSet<DirectoryReference> SystemIncludePaths = new();
		public HashSet<DirectoryReference> UserIncludePaths = new();

		public List<XcodeSourceFile> Files = new();
		public List<UEBuildModuleCPP> Modules = new List<UEBuildModuleCPP>();

		public FileReference ResponseFile;

		public UnrealBatchedFiles(UnrealData UnrealData, int Index)
		{
			ResponseFile = FileReference.Combine(UnrealData.XcodeProjectFileLocation.ParentDirectory!, "ResponseFiles", $"{UnrealData.ProductName}{Index}.response");
		}

		public void GenerateResponseFile()
		{
			StringBuilder ResponseFileContents = new();
			ResponseFileContents.Append("-I");
			ResponseFileContents.AppendJoin(" -I", SystemIncludePaths.Select(x => x.FullName.Contains(' ') ? $"\"{x.FullName}\"" : x.FullName));
			ResponseFileContents.Append(" -I");
			ResponseFileContents.AppendJoin(" -I", UserIncludePaths.Select(x => x.FullName.Contains(' ') ? $"\"{x.FullName}\"" : x.FullName));

			ResponseFileContents.Append(" -D");
			ResponseFileContents.AppendJoin(" -D", AllDefines);

			if (PCHFile != null)
			{
				ResponseFileContents.Append($" -include {PCHFile.FullName}");
			}

			ResponseFileContents.Append(bEnableRTTI ? " -fno-rtti" : " -frtti");

			DirectoryReference.CreateDirectory(ResponseFile.Directory);
			FileReference.WriteAllText(ResponseFile, ResponseFileContents.ToString());
		}
	}

	enum MetadataPlatform
	{
		MacEditor,
		Mac,
		IOS,
	}

	enum MetadataMode
	{
		UsePremade,
		UpdateTemplate,
	}

	class Metadata
	{
		public MetadataMode Mode;
		public FileReference File;
		public string XcodeProjectRelative;

		//public Metadata(MetadataMode Mode, FileReference File, DirectoryReference XcodeProjectFile)
		//{
		//	this.Mode = Mode;
		//	this.File = File;
		//	XcodeProjectRelative = File.MakeRelativeTo(XcodeProjectFile.ParentDirectory!);
		//}

		public Metadata(DirectoryReference ProductDirectory, DirectoryReference XcodeProject, ConfigHierarchy Ini, string IniKey, string StandardLocationSubdir, string? ProjectTemplateSubdir = null)
		{
			string? PremadeLocation;
			Ini.TryGetValue("XcodeConfiguration", IniKey, out PremadeLocation);

			if (!string.IsNullOrEmpty(PremadeLocation))
			{
				Mode = MetadataMode.UsePremade;
				File = FileReference.Combine(ProductDirectory, PremadeLocation);
			}
			else
			{
				if (File == null)
				{
					FileReference StandardProjectLocation = FileReference.Combine(ProductDirectory, StandardLocationSubdir);
					if (FileReference.Exists(StandardProjectLocation))
					{
						Mode = MetadataMode.UsePremade;
						File = StandardProjectLocation;
					}
					else
					{
						Mode = MetadataMode.UpdateTemplate;
						if (ProjectTemplateSubdir != null)
						{
							File = FileReference.Combine(ProductDirectory, ProjectTemplateSubdir);
						}
						else
						{
							File = FileReference.Combine(Unreal.EngineDirectory, StandardLocationSubdir);
						}
					}
				}
			}

			XcodeProjectRelative = File.MakeRelativeTo(XcodeProject.ParentDirectory!);
		}
	}


	class UnrealData
	{
		public bool bIsAppBundle;
		public bool bHasEditorConfiguration;
		public bool bUseAutomaticSigning = false;
		public bool bIsMergingProjects = false;
		public bool bWriteCodeSigningSettings = true;

		public Dictionary<MetadataPlatform, Metadata> PlistFiles = new();
		public Dictionary<MetadataPlatform, Metadata> EntitlementsFiles = new();

		public List<UnrealBuildConfig> AllConfigs = new();

		public List<UnrealBatchedFiles> BatchedFiles = new();
 
		public FileReference? UProjectFileLocation = null;
		public DirectoryReference XcodeProjectFileLocation;
		public DirectoryReference ProductDirectory;

		// settings read from project configs
		public IOSProjectSettings? IOSProjectSettings;
		public IOSProvisioningData? IOSProvisioningData;

		public TVOSProjectSettings? TVOSProjectSettings;
		public TVOSProvisioningData? TVOSProvisioningData;

		// Name of the product (usually the project name, but UE5.xcodeproj is actually UnrealGame product)
		public string ProductName;

		// Display name, can be overridden from commandline
		public string DisplayName;


		/// <summary>
		///  Used to mark the project for distribution (some platforms require this)
		/// </summary>
		public bool bForDistribution = false;

		/// <summary>
		/// Override for bundle identifier
		/// </summary>
		public string BundleIdentifier = "";

		/// <summary>
		/// Override AppName
		/// </summary>
		public string AppName = "";

		/// <summary>
		/// Architectures supported for iOS
		/// </summary>
		public string[] SupportedIOSArchitectures = { "arm64" };

		/// <summary>
		/// UBT logger object
		/// </summary>
		public ILogger? Logger;

		private XcodeProjectFile? ProjectFile;

		public static bool bSupportsMac => Supports(UnrealTargetPlatform.Mac);
		public static bool bSupportsIOS => Supports(UnrealTargetPlatform.IOS);
		public static bool bSupportsTVOS => Supports(UnrealTargetPlatform.TVOS);
		public static bool Supports(UnrealTargetPlatform? Platform)
		{
			return Platform == null || XcodeProjectFileGenerator.XcodePlatforms.Contains((UnrealTargetPlatform)Platform);
		}


		public UnrealData(FileReference XcodeProjectFileLocation, bool bIsForDistribution, string BundleID, string AppName)
		{
			// the .xcodeproj is actually a directory
			this.XcodeProjectFileLocation = new DirectoryReference(XcodeProjectFileLocation.FullName);
			// default to engine director, will be fixed in Initialize if needed
			ProductDirectory = Unreal.EngineDirectory;
			ProductName = XcodeProjectFileLocation.GetFileNameWithoutAnyExtensions();
			if (ProductName == "UE5")
			{
				ProductName = "UnrealGame";
			}

			this.bForDistribution = bIsForDistribution;
			this.BundleIdentifier = string.IsNullOrEmpty(BundleID) ? "$(UE_SIGNING_PREFIX).$(UE_PRODUCT_NAME_STRIPPED)" : BundleID;
			this.DisplayName = string.IsNullOrEmpty(AppName) ? "$(UE_PRODUCT_NAME)" : BundleID; ;
		}

		public FileReference? FindUProjectFileLocation(XcodeProjectFile ProjectFile)
		{
			// find a uproject file (UE5 target won't have one)
			foreach (Project Target in ProjectFile.ProjectTargets)
			{
				if (Target.UnrealProjectFilePath != null)
				{
					UProjectFileLocation = Target.UnrealProjectFilePath;
					break;
				}
			}

			// now that we have a UProject file (or not), update the FileCollection RootDirectory to point to it
			ProjectFile.FileCollection.SetUProjectLocation(UProjectFileLocation);

			return UProjectFileLocation;
		}

		public bool Initialize(XcodeProjectFile ProjectFile, List<UnrealTargetConfiguration> Configurations, ILogger Logger)
		{
			this.ProjectFile = ProjectFile;
			this.Logger = Logger;

			FindUProjectFileLocation(ProjectFile);

			// make sure ProjectDir is something good
			if (UProjectFileLocation != null)
			{
				ProductDirectory = UProjectFileLocation.Directory;
			}
			else if (ProjectFile.ProjectTargets[0].TargetRules!.Type == TargetType.Program)
			{
				DirectoryReference? ProgramFinder = DirectoryReference.Combine(ProjectFile.BaseDir);
				while (ProgramFinder != null && string.Compare(ProgramFinder.GetDirectoryName(), "Source", true) != 0)
				{
					ProgramFinder = ProgramFinder.ParentDirectory;
				}
				// we are now at Source directory, go up one more, then into Programs, and finally the "project" directory
				if (ProgramFinder != null)
				{
					ProgramFinder = DirectoryReference.Combine(ProgramFinder, "../Programs", ProductName);
					// if it exists, we have a ProductDir we can use for plists, icons, etc
					if (DirectoryReference.Exists(ProgramFinder))
					{
						ProductDirectory = ProgramFinder;
					}
				}
			}

			InitializeMetadata();

			// Figure out all the desired configurations on the unreal side
			AllConfigs = GetSupportedBuildConfigs(XcodeProjectFileGenerator.XcodePlatforms, Configurations, Logger);
			// if we can't find any configs, we will fail to create a project
			if (AllConfigs.Count == 0)
			{
				return false;
			}

			// this project makes an app bundle (.app directory instead of a raw executable or dylib) if none of the fings make a non-appbundle
			bIsAppBundle = !AllConfigs.Any(x => x.ProjectTarget!.TargetRules!.bIsBuildingConsoleApplication || x.ProjectTarget.TargetRules.bShouldCompileAsDLL);
			bHasEditorConfiguration = AllConfigs.Any(x => x.ProjectTarget!.TargetRules!.Type == TargetType.Editor);

			// read config settings
			if (AllConfigs.Any(x => x.bSupportsIOS))
			{
				IOSPlatform IOSPlatform = ((IOSPlatform)UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.IOS));
				IOSProjectSettings = IOSPlatform.ReadProjectSettings(UProjectFileLocation);
				if (!bUseAutomaticSigning)
				{
					IOSProvisioningData = IOSPlatform.ReadProvisioningData(IOSProjectSettings, bForDistribution);
				}
			}

			if (AllConfigs.Any(x => x.bSupportsTVOS))
			{
				TVOSPlatform TVOSPlatform = ((TVOSPlatform)UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.TVOS));
				TVOSProjectSettings = TVOSPlatform.ReadProjectSettings(UProjectFileLocation);
				if (!bUseAutomaticSigning)
				{
					TVOSProvisioningData = TVOSPlatform.ReadProvisioningData(TVOSProjectSettings, bForDistribution);
				}
			}

			return true;
		}

		private void InitializeMetadata()
		{

			// read setings from the configs, now that we have a project
			ConfigHierarchy SharedPlatformIni = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, UProjectFileLocation?.Directory, UnrealTargetPlatform.Mac);
			SharedPlatformIni.TryGetValue("XcodeConfiguration", "bUseModernCodeSigning", out bUseAutomaticSigning);

			if (bSupportsMac)
			{
				PlistFiles[MetadataPlatform.MacEditor] = new Metadata(ProductDirectory, XcodeProjectFileLocation, SharedPlatformIni, "PremadeMacEditorPlist", "Build/Mac/Resources/Info-Editor.plist");
				PlistFiles[MetadataPlatform.Mac] = new Metadata(ProductDirectory, XcodeProjectFileLocation, SharedPlatformIni, "PremadeMacPlist", "Build/Mac/Resources/Info.plist");
				EntitlementsFiles[MetadataPlatform.MacEditor] = new Metadata(ProductDirectory, XcodeProjectFileLocation, SharedPlatformIni, "PremadeMacEditorPlist", "Build/Mac/Resources/Info-Editor.plist");
				EntitlementsFiles[MetadataPlatform.Mac] = new Metadata(ProductDirectory, XcodeProjectFileLocation, SharedPlatformIni, "PremadeMacPlist", "Build/Mac/Resources/.plist");
			}
			if (bSupportsIOS || bSupportsTVOS)
			{
				PlistFiles[MetadataPlatform.IOS] = new Metadata(ProductDirectory, XcodeProjectFileLocation, SharedPlatformIni, "PremadeIOSPlist", "Build/IOS/Resources/Info.plist", "Build/IOS/UBTGenerated/Info.plist");
				EntitlementsFiles[MetadataPlatform.IOS] = new Metadata(ProductDirectory, XcodeProjectFileLocation, SharedPlatformIni, "PremadeIOSPlist", "Build/IOS/Resources/Info.plist");
			}
		}

		public string? FindFile(List<string> Paths, UnrealTargetPlatform Platform, bool bMakeRelative)
		{
			foreach (string Entry in Paths)
			{
				string FinalPath = Entry.Replace("$(Engine)", Unreal.EngineDirectory.FullName);
				FinalPath = FinalPath.Replace("$(Project)", ProductDirectory.FullName);
				FinalPath = FinalPath.Replace("$(Platform)", Platform.ToString());

//				Console.WriteLine($"Looking for {FinalPath}");
				if (File.Exists(FinalPath) || Directory.Exists(FinalPath))
				{
//					Console.WriteLine($"  Found it!");
					if (bMakeRelative)
					{
						FinalPath = new FileReference(FinalPath).MakeRelativeTo(XcodeProjectFileLocation.ParentDirectory!);
					}
					return FinalPath;
				}
			}
			return null;
		}

		public string ProjectOrEnginePath(string SubPath, bool bMakeRelative)
		{
			string? FinalPath = null;
			if (UProjectFileLocation != null)
			{
				string PathToCheck = Path.Combine(ProductDirectory.FullName, SubPath);
				if (File.Exists(PathToCheck) || Directory.Exists(PathToCheck))
				{
					FinalPath = PathToCheck;
				}
			}

			if (FinalPath == null)
			{
				FinalPath = Path.Combine(Unreal.EngineDirectory.FullName, SubPath);
			}
			if (bMakeRelative)
			{
				FinalPath = new FileReference(FinalPath).MakeRelativeTo(XcodeProjectFileLocation.ParentDirectory!);
			}

			return FinalPath;
		}

		public void AddModule(UEBuildModuleCPP Module, CppCompileEnvironment CompileEnvironment)
		{
			// we need to keep all _API defines, but we can gather them up to append at the end, instead of using them to compute sameness

			// remove some extraneous defines that will cause every module to be unique, but we can do without
			List<string> Defines = new();
			List<string> APIDefines = new();
			Regex Regex = new Regex("#define ([A-Z0-9_]*) ?(.*)?");

			string DefinesString = "";
			FileReference? PCHFile = null;
			if (CompileEnvironment.ForceIncludeFiles.Count == 0)
			{
				// if there are no ForceInclude files, then that means it's a module that forces the includes to come from a generated PCH file
				// and so we will use this for definitions and uniqueness
				if (CompileEnvironment.PrecompiledHeaderIncludeFilename != null)
				{
					PCHFile = FileReference.Combine(XcodeProjectFileLocation.ParentDirectory!, "PCHFiles", CompileEnvironment.PrecompiledHeaderIncludeFilename.GetFileName());
					DirectoryReference.CreateDirectory(PCHFile.Directory);
					FileReference.Copy(CompileEnvironment.PrecompiledHeaderIncludeFilename, PCHFile, true);
				}
			}
			else
			{
				foreach (string Line in File.ReadAllLines(CompileEnvironment.ForceIncludeFiles.First(x => x.FullName.Contains("Definitions")).FullName))
				{
					Match Match = Regex.Match(Line);
					if (!Match.Success)
					{
						continue;
					}

					string Key = Match.Groups[1].Value;
					string Value = Match.Groups[2].Value;
					// if no value, then just define with no = stuff
					if (!Match.Groups[2].Success)
					{
						Defines.Add(Key);
						continue;
					}

					// skip some known per-module defines we don't need
					if (Key.StartsWith("UE_MODULE_NAME") || Key.StartsWith("UE_PLUGIN_NAME") || Key.StartsWith("UBT_MODULE_MANIFEST"))
					{
						continue;
					}
					// these API ones are per module but still need to be defined, can be defined to nothing
					else if (Key.Contains("_API"))
					{
						APIDefines.Add($"{Key}=");
					}
					else
					{
						// if the value is normal, just add the define as is
						if (Value.All(x => char.IsLetterOrDigit(x) || x == '_'))
						{
							Defines.Add($"{Key}={Value}");
						}
						else
						{
							// escape any quotes in the define, then quote the whole thing
							Value = Value.Replace("\"", "\\\"");
							Defines.Add($"{Key}=\"{Value}\"");
						}
					}
				}

				// sort and joing them into a single string to act as a key (and happily string to put into .xcconfig)
				Defines.Sort();
				DefinesString = string.Join(" ", Defines);
			}

			// now find a matching SubTarget, and make a new one if needed
			UnrealBatchedFiles? FileBatch = null;
			foreach (UnrealBatchedFiles Search in BatchedFiles)
			{
				if (Search.Definitions == DefinesString && Search.PCHFile == PCHFile &&
					Search.bEnableRTTI == CompileEnvironment.bUseRTTI)
				{
					FileBatch = Search;
					break;
				}
			}
			if (FileBatch == null)
			{
				FileBatch = new UnrealBatchedFiles(this, BatchedFiles.Count + 1);
				BatchedFiles.Add(FileBatch);
				FileBatch.Definitions = DefinesString;
				FileBatch.PCHFile = PCHFile;
				FileBatch.bEnableRTTI = CompileEnvironment.bUseRTTI;
			}

			FileBatch.Modules.Add(Module);
			FileBatch.AllDefines.UnionWith(Defines);
			FileBatch.AllDefines.UnionWith(APIDefines);
			FileBatch.SystemIncludePaths.UnionWith(CompileEnvironment.SystemIncludePaths);
			FileBatch.UserIncludePaths.UnionWith(CompileEnvironment.UserIncludePaths);
		}

		private List<UnrealBuildConfig> GetSupportedBuildConfigs(List<UnrealTargetPlatform> Platforms, List<UnrealTargetConfiguration> Configurations, ILogger Logger)
		{
			List<UnrealBuildConfig> BuildConfigs = new List<UnrealBuildConfig>();

			//string ProjectName = ProjectFilePath.GetFileNameWithoutExtension();

			foreach (UnrealTargetConfiguration Configuration in Configurations)
			{
				if (InstalledPlatformInfo.IsValidConfiguration(Configuration, EProjectType.Code))
				{
					foreach (UnrealTargetPlatform Platform in Platforms)
					{
						if (InstalledPlatformInfo.IsValidPlatform(Platform, EProjectType.Code) && (Platform == UnrealTargetPlatform.Mac || Platform == UnrealTargetPlatform.IOS || Platform == UnrealTargetPlatform.TVOS)) // @todo support other platforms
						{
							UEBuildPlatform? BuildPlatform;
							if (UEBuildPlatform.TryGetBuildPlatform(Platform, out BuildPlatform) && (BuildPlatform.HasRequiredSDKsInstalled() == SDKStatus.Valid))
							{
								// Check we have targets (Expected to be no Engine targets when generating for a single .uproject)
								if (ProjectFile!.ProjectTargets.Count == 0 && ProjectFile!.BaseDir != Unreal.EngineDirectory)
								{
									throw new BuildException($"Expecting at least one ProjectTarget to be associated with project '{XcodeProjectFileLocation}' in the TargetProjects list ");
								}

								// Now go through all of the target types for this project
								foreach (ProjectTarget ProjectTarget in ProjectFile.ProjectTargets.OfType<ProjectTarget>())
								{
									if (MSBuildProjectFile.IsValidProjectPlatformAndConfiguration(ProjectTarget, Platform, Configuration, Logger))
									{
										// Figure out if this is a monolithic build
										bool bShouldCompileMonolithic = BuildPlatform.ShouldCompileMonolithicBinary(Platform);
										bShouldCompileMonolithic |= (ProjectTarget.CreateRulesDelegate(Platform, Configuration).LinkType == TargetLinkType.Monolithic);

										string ConfigName = Configuration.ToString();
										if (ProjectTarget.TargetRules!.Type != TargetType.Game && ProjectTarget.TargetRules.Type != TargetType.Program)
										{
											ConfigName += " " + ProjectTarget.TargetRules.Type.ToString();
										}

										if (BuildConfigs.Where(Config => Config.DisplayName == ConfigName).ToList().Count == 0)
										{
											string TargetName = ProjectTarget.TargetFilePath.GetFileNameWithoutAnyExtensions();
											// Get the .uproject directory
											DirectoryReference? UProjectDirectory = DirectoryReference.FromFile(ProjectTarget.UnrealProjectFilePath);

											// Get the output directory
											DirectoryReference RootDirectory;
											if (UProjectDirectory != null && 
												(bShouldCompileMonolithic || ProjectTarget.TargetRules.BuildEnvironment == TargetBuildEnvironment.Unique) && 
												ProjectTarget.TargetRules.File!.IsUnderDirectory(UProjectDirectory))
											{
												RootDirectory = UEBuildTarget.GetOutputDirectoryForExecutable(UProjectDirectory, ProjectTarget.TargetRules.File!);
											}
											else
											{
												RootDirectory = UEBuildTarget.GetOutputDirectoryForExecutable(Unreal.EngineDirectory, ProjectTarget.TargetRules.File!);
											}

											string ExeName = TargetName;
											if (!bShouldCompileMonolithic && ProjectTarget.TargetRules.Type != TargetType.Program)
											{
												// Figure out what the compiled binary will be called so that we can point the IDE to the correct file
												if (ProjectTarget.TargetRules.Type != TargetType.Game)
												{
													// Only if shared - unique retains the Target Name
													if (ProjectTarget.TargetRules.BuildEnvironment == TargetBuildEnvironment.Shared)
													{
														ExeName = "Unreal" + ProjectTarget.TargetRules.Type.ToString();
													}
												}
											}

											// Get the output directory
											DirectoryReference OutputDirectory = DirectoryReference.Combine(RootDirectory, "Binaries");
											DirectoryReference MacBinaryDir = DirectoryReference.Combine(OutputDirectory, "Mac");
											DirectoryReference IOSBinaryDir = DirectoryReference.Combine(OutputDirectory, "IOS");
											DirectoryReference TVOSBinaryDir = DirectoryReference.Combine(OutputDirectory, "TVOS");
											if (!string.IsNullOrEmpty(ProjectTarget.TargetRules.ExeBinariesSubFolder))
											{
												MacBinaryDir = DirectoryReference.Combine(MacBinaryDir, ProjectTarget.TargetRules.ExeBinariesSubFolder);
												IOSBinaryDir = DirectoryReference.Combine(IOSBinaryDir, ProjectTarget.TargetRules.ExeBinariesSubFolder);
												TVOSBinaryDir = DirectoryReference.Combine(TVOSBinaryDir, ProjectTarget.TargetRules.ExeBinariesSubFolder);
											}

											if (BuildPlatform.Platform == UnrealTargetPlatform.Mac)
											{
												string MacExecutableName = MakeExecutableFileName(ExeName, UnrealTargetPlatform.Mac, Configuration, ProjectTarget.TargetRules.Architectures, ProjectTarget.TargetRules.UndecoratedConfiguration);
												string IOSExecutableName = MacExecutableName.Replace("-Mac-", "-IOS-");
												string TVOSExecutableName = MacExecutableName.Replace("-Mac-", "-TVOS-");
												BuildConfigs.Add(new UnrealBuildConfig(ConfigName, TargetName, FileReference.Combine(MacBinaryDir, MacExecutableName), FileReference.Combine(IOSBinaryDir, IOSExecutableName), FileReference.Combine(TVOSBinaryDir, TVOSExecutableName), ProjectTarget, Configuration));
											}
											else if (BuildPlatform.Platform == UnrealTargetPlatform.IOS || BuildPlatform.Platform == UnrealTargetPlatform.TVOS)
											{
												string IOSExecutableName = MakeExecutableFileName(ExeName, UnrealTargetPlatform.IOS, Configuration, ProjectTarget.TargetRules.Architectures, ProjectTarget.TargetRules.UndecoratedConfiguration);
												string TVOSExecutableName = IOSExecutableName.Replace("-IOS-", "-TVOS-");
												//string MacExecutableName = IOSExecutableName.Replace("-IOS-", "-Mac-");
												BuildConfigs.Add(new UnrealBuildConfig(ConfigName, TargetName, FileReference.Combine(MacBinaryDir, IOSExecutableName), FileReference.Combine(IOSBinaryDir, IOSExecutableName), FileReference.Combine(TVOSBinaryDir, TVOSExecutableName), ProjectTarget, Configuration));
											}
										}
									}
								}
							}
						}
					}
				}
			}

			return BuildConfigs;
		}

		public static IEnumerable<UnrealTargetConfiguration> GetSupportedConfigurations()
		{
			return new UnrealTargetConfiguration[] {
				UnrealTargetConfiguration.Debug,
				UnrealTargetConfiguration.DebugGame,
				UnrealTargetConfiguration.Development,
				UnrealTargetConfiguration.Test,
				UnrealTargetConfiguration.Shipping
			};
		}

		public static bool ShouldIncludeProjectInWorkspace(ProjectFile Proj, ILogger Logger)
		{
			return CanBuildProjectLocally(Proj, Logger);
		}

		public static bool CanBuildProjectLocally(ProjectFile Proj, ILogger Logger)
		{
			foreach (Project ProjectTarget in Proj.ProjectTargets)
			{
				foreach (UnrealTargetPlatform Platform in XcodeProjectFileGenerator.XcodePlatforms)
				{
					foreach (UnrealTargetConfiguration Config in GetSupportedConfigurations())
					{
						if (MSBuildProjectFile.IsValidProjectPlatformAndConfiguration(ProjectTarget, Platform, Config, Logger))
						{
							return true;
						}
					}
				}
			}

			return false;
		}

		private static string MakeExecutableFileName(string BinaryName, UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration, UnrealArchitectures Architectures, UnrealTargetConfiguration UndecoratedConfiguration)
		{
			StringBuilder Result = new StringBuilder();

			Result.Append(BinaryName);

			if (Configuration != UndecoratedConfiguration)
			{
				Result.AppendFormat("-{0}-{1}", Platform.ToString(), Configuration.ToString());
			}

			UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(Platform);
			if (UnrealArchitectureConfig.ForPlatform(Platform).RequiresArchitectureFilenames(Architectures))
			{
				Result.Append(Architectures.ToString());
			}

			return Result.ToString();
		}


		// cache for the below function
		Dictionary<string, UnrealArchitectures> CachedMacProjectArchitectures = new();

		/// <summary>
		/// Returns the Mac architectures that should be configured for the provided target. If the target has a project we'll adhere
		/// to whether it's set as Intel/Universal/Apple unless the type is denied (pretty much just Editor)
		/// 
		/// If the target has no project we'll support allow-listed targets for installed builds and all non-editor architectures 
		/// for source builds. Not all programs are going to compile for Apple Silicon, but being able to build and fail is useful...
		/// </summary>
		/// <param name="Config">Build config for the target we're generating</param>
		/// <param name="InProjectFile">Path to the project file, or null if the target has no project</param>
		/// <returns></returns>
		public UnrealArchitectures GetSupportedMacArchitectures(UnrealBuildConfig Config, FileReference? InProjectFile)
		{
			// All architectures supported
			UnrealArchitectures AllArchitectures = new(new[] { UnrealArch.X64, UnrealArch.Arm64 });

			// Add a way on the command line of forcing a project file with all architectures (there isn't a good way to let this be
			// set and checked where we can access it).
			bool ForceAllArchitectures = Environment.GetCommandLineArgs().Contains("AllArchitectures", StringComparer.OrdinalIgnoreCase);

			if (ForceAllArchitectures)
			{
				return AllArchitectures;
			}

			string TargetName = Config.BuildTarget;

			// First time seeing this target?
			if (!CachedMacProjectArchitectures.ContainsKey(TargetName))
			{
				CachedMacProjectArchitectures[TargetName] = UnrealArchitectureConfig.ForPlatform(UnrealTargetPlatform.Mac).ProjectSupportedArchitectures(InProjectFile, TargetName);
			}

			return CachedMacProjectArchitectures[TargetName];
		}

	}

	/// <summary>
	/// Info needed to make a file a member of specific group
	/// </summary>
	class XcodeSourceFile : ProjectFile.SourceFile
	{
		/// <summary>
		/// Constructor
		/// </summary>
		public XcodeSourceFile(FileReference InitFilePath, DirectoryReference? InitRelativeBaseFolder)
			: base(InitFilePath, InitRelativeBaseFolder)
		{
			FileGuid = XcodeProjectFileGenerator.MakeXcodeGuid();
			FileRefGuid = XcodeProjectFileGenerator.MakeXcodeGuid();
		}

		/// <summary>
		/// File Guid for use in Xcode project
		/// </summary>
		public string FileGuid
		{
			get;
			private set;
		}

		public void ReplaceGuids(string NewFileGuid, string NewFileRefGuid)
		{
			FileGuid = NewFileGuid;
			FileRefGuid = NewFileRefGuid;
		}

		/// <summary>
		/// File reference Guid for use in Xcode project
		/// </summary>
		public string FileRefGuid
		{
			get;
			private set;
		}
	}

	/// <summary>
	/// Represents a group of files shown in Xcode's project navigator as a folder
	/// </summary>
	internal class XcodeFileGroup
	{
		public XcodeFileGroup(string InName, string InPath, bool InIsReference)
		{
			GroupName = InName;
			GroupPath = InPath;
			GroupGuid = XcodeProjectFileGenerator.MakeXcodeGuid();
			bIsReference = InIsReference;
		}

		public XcodeFileGroup(XcodeFileGroup Other)
		{
			GroupName = Other.GroupName;
			GroupPath = Other.GroupPath;
			GroupGuid = Other.GroupGuid;
			bIsReference = Other.bIsReference;
			Files.AddRange(Other.Files);
			foreach (string Key in Other.Children.Keys)
			{
				Children.Add(Key, new XcodeFileGroup(Other.Children[Key]));
			}
		}

		public string GroupGuid;
		public string GroupName;
		public string GroupPath;
		public Dictionary<string, XcodeFileGroup> Children = new Dictionary<string, XcodeFileGroup>();
		public List<XcodeSourceFile> Files = new List<XcodeSourceFile>();
		public bool bIsReference;
	}

	class XcconfigFile
	{
		public string Name;
		public string Guid;
		public FileReference FileRef;
		internal StringBuilder Text;

		public XcconfigFile(DirectoryReference XcodeProjectDirectory, UnrealTargetPlatform? Platform, string ConfigName)
		{
			Name = ConfigName;
			FileRef = FileReference.Combine(XcodeProjectDirectory, "Xcconfigs" + (Platform == null ? "" : Platform.ToString()), $"{ConfigName}.xcconfig");
			Guid = XcodeProjectFileGenerator.MakeXcodeGuid();
			Text = new StringBuilder();
		}

		public void AppendLine(string Line)
		{
			Text.Append(Line);
			Text.Append(ProjectFileGenerator.NewLine);
		}

		public void Write()
		{
			// write the file to disk
			DirectoryReference.CreateDirectory(FileRef.Directory);
			FileReference.WriteAllTextIfDifferent(FileRef, Text.ToString());
		}
	}




	abstract class XcodeProjectNode
	{
		// keeps a list of other node this node references, which is used when writing out the whole xcode project file
		public List<XcodeProjectNode> References = new();

		// optional Xcconfig file 
		public XcconfigFile? Xcconfig = null;

		/// <summary>
		/// Abstract function the individual node classes must override to write out the node to the project file
		/// </summary>
		/// <param name="Content"></param>
		public abstract void Write(StringBuilder Content);


		/// <summary>
		/// Walks the references of the given node to find all nodes of the given type. 
		/// </summary>
		/// <typeparam name="T">Parent class of the nodes to return</typeparam>
		/// <param name="Node">Root node to start with</param>
		/// <returns>Set of matching nodes</returns>
		public static IEnumerable<T> GetNodesOfType<T>(XcodeProjectNode Node) where T : XcodeProjectNode
		{
			// gather the nodes without recursion
			LinkedList<XcodeProjectNode> Nodes = new();
			Nodes.AddLast(Node);

			// pull off the front of the "deque" amd add its references to the back, gather
			List<XcodeProjectNode> Return = new();
			while (Nodes.Count() > 0)
			{
				XcodeProjectNode Head = Nodes.First();
				Nodes.RemoveFirst();
				Head.References.ForEach(x => Nodes.AddLast(x));

				// remember them all 
				Return.AddRange(Head.References);
			}

			// filter down
			return Return.OfType<T>();
		}


		public void CreateXcconfigFile(XcodeProject Project, string Name)
		{
			DirectoryReference XcodeProjectDirectory = Project.UnrealData.XcodeProjectFileLocation.ParentDirectory!;
			Xcconfig = new XcconfigFile(XcodeProjectDirectory, Project.Platform, Name);
			Project.FileCollection.AddFileReference(Xcconfig.Guid, Xcconfig.FileRef.MakeRelativeTo(XcodeProjectDirectory), "explicitFileType", "test.xcconfig", "\"<group>\"", "Xcconfigs");
		}

		public virtual void WriteXcconfigFile()
		{
			
		}


		/// <summary>
		/// THhis will walk the node reference tree and call WRite on each node to add all needed nodes to the xcode poject file
		/// </summary>
		/// <param name="Content"></param>
		/// <param name="Node"></param>
		/// <param name="WrittenNodes"></param>
		public static void WriteNodeAndReferences(StringBuilder Content, XcodeProjectNode Node, HashSet<XcodeProjectNode>? WrittenNodes = null)
		{
			if (WrittenNodes == null)
			{
				WrittenNodes = new();
			}

			// write the node into the xcode project file
			Node.Write(Content);
			Node.WriteXcconfigFile();

			foreach (XcodeProjectNode Reference in Node.References)
			{
				if (!WrittenNodes.Contains(Reference))
				{
					WrittenNodes.Add(Reference);
					WriteNodeAndReferences(Content, Reference, WrittenNodes);
				}
			}	
		}
	}


	class XcodeDependency : XcodeProjectNode
	{
		public XcodeTarget Target;
		public string Guid = XcodeProjectFileGenerator.MakeXcodeGuid();
		public string ProxyGuid = XcodeProjectFileGenerator.MakeXcodeGuid();
		public string ProjectGuid;


		public XcodeDependency(XcodeTarget Target, string ProjectGuid)
		{
			this.Target = Target;
			this.ProjectGuid = ProjectGuid;

			References.Add(Target);
		}

		public override void Write(StringBuilder Content)
		{
			Content.WriteLine("/* Begin PBXContainerItemProxy section */");
			Content.WriteLine($"\t\t{ProxyGuid} /* PBXContainerItemProxy */ = {{");
			Content.WriteLine("\t\t\tisa = PBXContainerItemProxy;");
			Content.WriteLine($"\t\t\tcontainerPortal = {ProjectGuid} /* Project object */;");
			Content.WriteLine("\t\t\tproxyType = 1;");
			Content.WriteLine($"\t\t\tremoteGlobalIDString = {Target.Guid};");
			Content.WriteLine($"\t\t\tremoteInfo = \"{Target.Name}\";");
			Content.WriteLine("\t\t};");
			Content.WriteLine("/* End PBXContainerItemProxy section */");
			Content.WriteLine("");

			Content.WriteLine("/* Begin PBXTargetDependency section */");
			Content.WriteLine($"\t\t{Guid} /* PBXTargetDependency */ = {{");
			Content.WriteLine("\t\t\tisa = PBXTargetDependency;");
			Content.WriteLine($"\t\t\ttarget = {Target.Guid} /* {Target.Name} */;");
			Content.WriteLine($"\t\t\ttargetProxy = {ProxyGuid} /* PBXContainerItemProxy */;");
			Content.WriteLine("\t\t};");
			Content.WriteLine("/* End PBXTargetDependency section */");
		}
	}

	abstract class XcodeBuildPhase : XcodeProjectNode
	{
		public string Name;
		public string IsAType;
		public string Guid = XcodeProjectFileGenerator.MakeXcodeGuid();
		protected List<XcodeSourceFile> Items = new();

		public XcodeBuildPhase(string Name, string IsAType)
		{
			this.Name = Name;
			this.IsAType = IsAType;
		}

		public override void Write(StringBuilder Content)
		{
			Content.WriteLine($"/* Begin {IsAType} section */");
			Content.WriteLine(2, $"{Guid} = {{");
			Content.WriteLine(3,	$"isa = {IsAType};");
			Content.WriteLine(3,	"buildActionMask = 2147483647;");
			Content.WriteLine(3,	"files = (");
			foreach (XcodeSourceFile File in Items)
			{
				Content.WriteLine(4,		$"{File.FileGuid} /* {File.Reference.GetFileName()} in {Name} */,");
			}
			Content.WriteLine(3, ");");
			Content.WriteLine(3, "runOnlyForDeploymentPostprocessing = 0;");
			Content.WriteLine(2, "};");
			Content.WriteLine($"/* End {IsAType} section */");
		}
	}

	class XcodeSourcesBuildPhase : XcodeBuildPhase
	{
		public XcodeSourcesBuildPhase()
			: base("Sources", "PBXSourcesBuildPhase")
		{
		}

		public void AddFile(XcodeSourceFile File)
		{
			Items.Add(File);
		}

		public override void Write(StringBuilder Content)
		{
			Content.WriteLine("/* Begin PBXSourcesBuildPhase section */");
			Content.WriteLine($"\t\t{Guid} = {{");
			Content.WriteLine("\t\t\tisa = PBXSourcesBuildPhase;");
			Content.WriteLine("\t\t\tbuildActionMask = 2147483647;");
			Content.WriteLine("\t\t\tfiles = (");
			foreach (XcodeSourceFile File in Items)
			{
				Content.WriteLine($"\t\t\t\t{File.FileGuid} /* {File.Reference.GetFileName()} in {Name} */,");
			}
			Content.WriteLine("\t\t\t);");
			Content.WriteLine("\t\t\trunOnlyForDeploymentPostprocessing = 0;");
			Content.WriteLine("\t\t};");
			Content.WriteLine("/* End PBXSourcesBuildPhase section */");
		}
	}

	class XcodeResourcesBuildPhase : XcodeBuildPhase
	{
		private XcodeFileCollection FileCollection;

		public XcodeResourcesBuildPhase(XcodeFileCollection FileCollection)
			: base("Resources", "PBXResourcesBuildPhase")
		{
			this.FileCollection = FileCollection;
		}

		public void AddResource(FileReference Resource)
		{
			XcodeSourceFile ResourceSource = new XcodeSourceFile(Resource, null);
			FileCollection.ProcessFile(ResourceSource, true);

			Items.Add(ResourceSource);
		}
	}



	class XcodeBuildConfig : XcodeProjectNode
	{
		public string Guid = XcodeProjectFileGenerator.MakeXcodeGuid();
		public UnrealBuildConfig Info;

		public XcodeBuildConfig(UnrealBuildConfig Info)
		{
			this.Info = Info;
		}

		public override void Write(StringBuilder Content)
		{
			Content.WriteLine(2, $"{Guid} /* {Info.DisplayName} */ = {{");
			Content.WriteLine(3,	"isa = XCBuildConfiguration;");
			if (Xcconfig != null)
			{
				Content.WriteLine(3,	$"baseConfigurationReference = {Xcconfig.Guid} /* {Xcconfig.Name}.xcconfig */;");
			}
			Content.WriteLine(3,	"buildSettings = {");
			Content.WriteLine(3,	"};");
			Content.WriteLine(3,	$"name = \"{Info.DisplayName}\";");
			Content.WriteLine(2, "};");
		}
	}
	
	class XcodeBuildConfigList : XcodeProjectNode
	{ 
		public string Guid = XcodeProjectFileGenerator.MakeXcodeGuid();
		public string TargetName;

		public List<XcodeBuildConfig> BuildConfigs = new();

		public bool bSupportsMac => Supports(UnrealTargetPlatform.Mac);
		public bool bSupportsIOS => Supports(UnrealTargetPlatform.IOS);
		public bool bSupportsTVOS => Supports(UnrealTargetPlatform.TVOS);
		public bool Supports(UnrealTargetPlatform? Platform)
		{
			return this.Platform == Platform || (this.Platform == null && BuildConfigs.Any(x => x.Info.Supports(Platform)));
		}

		private UnrealTargetPlatform? Platform;
		public XcodeBuildConfigList(UnrealTargetPlatform? Platform, string TargetName, List<UnrealBuildConfig> BuildConfigInfos)
		{
			this.Platform = Platform;

			if (BuildConfigInfos.Count == 0)
			{
				throw new BuildException("Created a XcodeBuildConfigList with no BuildConfigs. This likely means a target was created too early");
			}

			this.TargetName = TargetName;

			// create build config objects for each info passed in, and them as references
			IEnumerable<XcodeBuildConfig> Configs = BuildConfigInfos.Select(x => new XcodeBuildConfig(x));
			// filter out configs that dont match a platform if we are single-platform mode
			Configs = Configs.Where(x => Platform == null || x.Info.Supports((UnrealTargetPlatform)Platform));
			BuildConfigs = Configs.ToList();
			References.AddRange(BuildConfigs);
		}

		public override void Write(StringBuilder Content)
		{
			// figure out the default configuration to use
			string Default = BuildConfigs.Any(x => x.Info.DisplayName.Contains(" Editor")) ? "Development Editor" : "Development";

			Content.WriteLine(2, $"{Guid} /* Build configuration list for target {TargetName} */ = {{");
			Content.WriteLine(3,	"isa = XCConfigurationList;");
			Content.WriteLine(3,	"buildConfigurations = (");
			foreach (XcodeBuildConfig Config in BuildConfigs)
			{
				Content.WriteLine(4,		$"{Config.Guid} /* {Config.Info.DisplayName} */,");
			}
			Content.WriteLine(3,	");");
			Content.WriteLine(3,	"defaultConfigurationIsVisible = 0;");
			Content.WriteLine(3,	$"defaultConfigurationName = \"{Default}\";");
			Content.WriteLine(2, "};");
		}
	}

	class XcodeTarget : XcodeProjectNode
	{
		public enum Type
		{
			Run_App,
			Run_Tool,
			Build,
			Index,
		}

		// Guid for this target
		public string Guid = XcodeProjectFileGenerator.MakeXcodeGuid();
//		string TargetAppGuid = XcodeProjectFileGenerator.MakeXcodeGuid();

		// com.apple.product-type.application, etc
		string ProductType;

		// xcode target type name
		string TargetTypeName;
		Type TargetType;

		// UE5_Build, EngineTest_SubIndex1, etc
		public string Name;

		// list of build configs this target supports (for instance, the Index target only indexes a Development config) 
		public XcodeBuildConfigList? BuildConfigList;

		// dependencies for this target
		public List<XcodeDependency> Dependencies = new List<XcodeDependency>();

		// build phases for this target (source, resource copying, etc)
		public List<XcodeBuildPhase> BuildPhases = new List<XcodeBuildPhase>();

		private FileReference? GameProject;

		public XcodeTarget(Type Type, UnrealData UnrealData, string? OverrideName=null)
		{
			GameProject = UnrealData.UProjectFileLocation;

			string ConfigName;
			TargetType = Type;
			switch (Type)
			{
				case Type.Run_App:
					ProductType = "com.apple.product-type.application";
					TargetTypeName = "PBXNativeTarget";
					ConfigName = "_Run";
					break;
				case Type.Run_Tool:
					ProductType = "com.apple.product-type.tool";
					TargetTypeName = "PBXNativeTarget";
					ConfigName = "_Run";
					break;
				case Type.Build:
					ProductType = "com.apple.product-type.library.static";
					TargetTypeName = "PBXLegacyTarget";
					ConfigName = "_Build";
					break;
				case Type.Index:
					ProductType = "com.apple.product-type.library.static";
					TargetTypeName = "PBXNativeTarget";
					ConfigName = "_Index";
					break;
				default:
					throw new BuildException($"Unhandled target type {Type}");
			}

			// set up names
			Name = UnrealData.ProductName + (OverrideName ?? ConfigName);
		}

		public void AddDependency(XcodeTarget Target, XcodeProject Project)
		{
			XcodeDependency Dependency = new XcodeDependency(Target, Project.Guid);
			Dependencies.Add(Dependency);
			References.Add(Dependency);
		}

		public override void Write(StringBuilder Content)
		{
			Content.WriteLine($"/* Begin {TargetType} section */");

			Content.WriteLine(2, $"{Guid} /* {Name} */ = {{");
			Content.WriteLine(3,	$"isa = {TargetTypeName};");
				
			Content.WriteLine(3,	$"buildConfigurationList = {BuildConfigList!.Guid} /* Build configuration list for {TargetTypeName} \"{Name}\" */;");

			if (TargetType == Type.Build)
			{
				// get paths to Unreal bits to be able ro tun UBT
				string UProjectParam = GameProject == null ? "" : $"{GameProject.FullName.Replace(" ", "\\ ")}";
				string UEDir = XcodeFileCollection.ConvertPath(Path.GetFullPath(Directory.GetCurrentDirectory() + "../../.."));
				string BuildToolPath = UEDir + "/Engine/Build/BatchFiles/Mac/XcodeBuild.sh";

				// insert elements to call UBT when building
				Content.WriteLine(3,	$"buildArgumentsString = \"$(ACTION) $(UE_TARGET_NAME) $(PLATFORM_NAME) $(UE_TARGET_CONFIG) {UProjectParam}\";");
				Content.WriteLine(3,	$"buildToolPath = \"{BuildToolPath}\";");
				Content.WriteLine(3,	$"buildWorkingDirectory = \"{UEDir}\";");
			}
			Content.WriteLine(3,	"buildPhases = (");
			foreach (XcodeBuildPhase BuildPhase in BuildPhases)
			{
				Content.WriteLine(4,		$"{BuildPhase.Guid} /* {BuildPhase.Name} */,");
			}
			Content.WriteLine(3,	");");
			Content.WriteLine(3,	"dependencies = (");
			foreach (XcodeDependency Dependency in Dependencies)
			{
				Content.WriteLine(4,		$"{Dependency.Guid} /* {Dependency.Target.Name} */,");
			}
			Content.WriteLine(3,	");");
			Content.WriteLine(3,	$"name = \"{Name}\";");
			Content.WriteLine(3,	"passBuildSettingsInEnvironment = 1;");
			Content.WriteLine(3,	$"productType = \"{ProductType}\";");
			WriteExtraTargetProperties(Content);
			Content.WriteLine(2, "};");

			Content.WriteLine($"/* End {TargetType} section */");
		}

		/// <summary>
		/// Let subclasses add extra properties into this target section
		/// </summary>
		protected virtual void WriteExtraTargetProperties(StringBuilder Content)
		{
			// nothing by default
		}
	}

	class XcodeRunTarget : XcodeTarget
	{
		private string ProductGuid = XcodeProjectFileGenerator.MakeXcodeGuid();
		private UnrealData UnrealData;
		public UnrealTargetPlatform Platform;

		public XcodeRunTarget(XcodeProject Project, UnrealTargetPlatform Platform, XcodeBuildTarget? BuildTarget)
			: base(Project.UnrealData.bIsAppBundle ? XcodeTarget.Type.Run_App : XcodeTarget.Type.Run_Tool, Project.UnrealData, 
				  XcodeProjectFileGenerator.PerPlatformMode == XcodePerPlatformMode.OneWorkspacePerPlatform ? "" : $"_{Platform}")
		{
			this.Platform = Platform;
			BuildConfigList = new XcodeBuildConfigList(Platform, Name, Project.UnrealData.AllConfigs);
			References.Add(BuildConfigList);

			// add the Product item to the project to be visible in left pane
			UnrealData = Project.UnrealData;
			Project.FileCollection.AddFileReference(ProductGuid, UnrealData.ProductName, "explicitFileType", Project.UnrealData.bIsAppBundle ? "wrapper.application" : "\"compiled.mach-o.executable\"", "BUILT_PRODUCTS_DIR", "Products");

			if (Project.UnrealData.bIsAppBundle)
			{
				XcodeResourcesBuildPhase ResourcesBuildPhase = new XcodeResourcesBuildPhase(Project.FileCollection);
				BuildPhases.Add(ResourcesBuildPhase);
				References.Add(ResourcesBuildPhase);

				List<string>StoryboardPaths = new List<string>()
				{
					"$(Project)/Build/$(Platform)/Resources/Interface/LaunchScreen.storyboardc",
					"$(Project)/Build/$(Platform)/Resources/Interface/LaunchScreen.storyboard",
					"$(Project)/Build/Apple/Resources/Interface/LaunchScreen.storyboardc",
					"$(Project)/Build/Apple/Resources/Interface/LaunchScreen.storyboard",
					"$(Engine)/Build/$(Platform)/Resources/Interface/LaunchScreen.storyboardc",
					"$(Engine)/Build/$(Platform)/Resources/Interface/LaunchScreen.storyboard",
					"$(Engine)/Build/Apple/Resources/Interface/LaunchScreen.storyboardc",
					"$(Engine)/Build/Apple/Resources/Interface/LaunchScreen.storyboard",
				};

				// look for Assets
				string? StoryboardPath;
				if (Platform == UnrealTargetPlatform.Mac)
				{
					string AssetsPath = UnrealData.ProjectOrEnginePath("Build/Mac/Resources/Assets.xcassets", false);
					ResourcesBuildPhase.AddResource(new FileReference(AssetsPath));
					StoryboardPath = UnrealData.FindFile(StoryboardPaths, UnrealTargetPlatform.Mac, false);
				}
				else
				{
					string AssetsPath = UnrealData.ProjectOrEnginePath("Build/IOS/Resources/Assets.xcassets", false);
					ResourcesBuildPhase.AddResource(new FileReference(AssetsPath));
					StoryboardPath = UnrealData.FindFile(StoryboardPaths, UnrealTargetPlatform.IOS, false);
				}

				if (StoryboardPath != null)
				{
					ResourcesBuildPhase.AddResource(new FileReference(StoryboardPath));
				}
			}

			if (BuildTarget != null)
			{
				AddDependency(BuildTarget, Project);
			}

			CreateXcconfigFile(Project, Name);

			// hook up each buildconfig to this Xcconfig 
			BuildConfigList!.BuildConfigs.ForEach(x => x.Xcconfig = Xcconfig);
		}

		protected override void WriteExtraTargetProperties(StringBuilder Content)
		{
			Content.WriteLine($"\t\t\tproductReference = {ProductGuid};");
			Content.WriteLine($"\t\t\tproductName = \"{UnrealData.ProductName}\";");
		}

		public override void WriteXcconfigFile()
		{
			// pull in the stuff that UBT updates
			DirectoryReference ProjectOrEngineDir = UnrealData.UProjectFileLocation?.Directory ?? Unreal.EngineDirectory;
			Xcconfig!.AppendLine($"#include? \"{ProjectOrEngineDir}/Intermediate/Build/Versions.xcconfig\"");
			Xcconfig.AppendLine("");

			// #jira UE-143619: Pre Monterey macOS requires this option for a packaged app to run on iOS15 due to new code signature format. Could be removed once Monterey is miniuS.
			Xcconfig.AppendLine("OTHER_CODE_SIGN_FLAGS = --generate-entitlement-der");

			Xcconfig.AppendLine($"MACOSX_DEPLOYMENT_TARGET = {MacToolChain.Settings.MacOSVersion};");
			Xcconfig.AppendLine("INFOPLIST_OUTPUT_FORMAT = xml");
			Xcconfig.AppendLine("COMBINE_HIDPI_IMAGES = YES");

			if (Platform == UnrealTargetPlatform.Mac)
			{
				Xcconfig.AppendLine($"SUPPORTED_PLATFORMS = macosx");
				Xcconfig.AppendLine($"SDKROOT = macosx");
			}
			else if (Platform == UnrealTargetPlatform.IOS)
			{
				Xcconfig.AppendLine($"SUPPORTED_PLATFORMS = iphoneos");// iphonesimulator");
				Xcconfig.AppendLine($"SDKROOT = iphoneos");
			}
			else
			{
				Xcconfig.AppendLine($"SUPPORTED_PLATFORMS = appletvos");
				Xcconfig.AppendLine($"SDKROOT = appletvos");
			}


			Xcconfig.Write();
		}
	}

	class XcodeBuildTarget : XcodeTarget
	{
		public XcodeBuildTarget(XcodeProject Project)
			: base(XcodeTarget.Type.Build, Project.UnrealData)
		{
			BuildConfigList = new XcodeBuildConfigList(Project.Platform, Name, Project.UnrealData.AllConfigs);
			References.Add(BuildConfigList);
		}
	}

	class XcodeIndexTarget : XcodeTarget
	{
		private UnrealData UnrealData;

		// just take the Project since it has everything we need, and is needed when adding target dependencies
		public XcodeIndexTarget(XcodeProject Project)
			: base(XcodeTarget.Type.Index, Project.UnrealData)
		{
			UnrealData = Project.UnrealData;

			BuildConfigList = new XcodeBuildConfigList(Project.Platform, Name, UnrealData.AllConfigs);
			References.Add(BuildConfigList);

			CreateXcconfigFile(Project, Name);
			// hook up each buildconfig to this Xcconfig 
			BuildConfigList.BuildConfigs.ForEach(x => x.Xcconfig = Xcconfig);


			// add all of the files to be natively compiled by this target
			XcodeSourcesBuildPhase SourcesBuildPhase = new XcodeSourcesBuildPhase();
			BuildPhases.Add(SourcesBuildPhase);
			References.Add(SourcesBuildPhase);

			foreach (KeyValuePair<XcodeSourceFile, FileReference?> Pair in Project.FileCollection.BuildableFilesToResponseFile)
			{
				// only add files that found a moduleto be part of (since we can't build without the build settings that come from a module)
				if (Pair.Value != null)
				{
					SourcesBuildPhase.AddFile(Pair.Key);
				}
			}
		}

		public override void WriteXcconfigFile()
		{
			// write out settings that apply whether or not we have subtargets, which #include this one, or no subtargets, and we write out more
			// @todo move tis to the subtarget and remember it from the Module
			Xcconfig!.AppendLine("CLANG_CXX_LANGUAGE_STANDARD = c++17");
			Xcconfig.AppendLine("GCC_WARN_CHECK_SWITCH_STATEMENTS = NO");
			Xcconfig.AppendLine("GCC_PRECOMPILE_PREFIX_HEADER = YES");
			Xcconfig.AppendLine("GCC_OPTIMIZATION_LEVEL = 0");
			Xcconfig.AppendLine($"PRODUCT_NAME = {Name}");
			Xcconfig.AppendLine("CONFIGURATION_BUILD_DIR = build");
			Xcconfig.Write();
		}
	}

	class XcodeProject : XcodeProjectNode
	{
		// a null platform here means all platforms like the old way
		public UnrealTargetPlatform? Platform;

		// the blob of data coming from unreal that we can pass around
		public UnrealData UnrealData;

		// container for all files and groups
		public XcodeFileCollection FileCollection;

		// Guid for the project node
		public string Guid = XcodeProjectFileGenerator.MakeXcodeGuid();
		private string ProvisioningStyle;

		public List<XcodeRunTarget> RunTargets = new();

		public XcodeBuildConfigList ProjectBuildConfigs;

		public XcodeProject(UnrealTargetPlatform? Platform, UnrealData UnrealData, XcodeFileCollection FileCollection)
		{
			this.Platform = Platform;
			this.UnrealData = UnrealData;
			this.FileCollection = FileCollection;

			ProvisioningStyle = UnrealData.bUseAutomaticSigning ? "Automatic" : "Manual";

			// if we are run-only, then we don't need a build target (this is shared between platforms if we are doing multi-target)
			XcodeBuildTarget? BuildTarget = null;
			if (!XcodeProjectFileGenerator.bGenerateRunOnlyProject)
			{
				// create a biuld target only if we have source files to build
				if (UnrealData.BatchedFiles.Count != 0)
				{
					BuildTarget = new XcodeBuildTarget(this);
				}
			}

			// create one run target for each platform if our platform is null (ie XcodeProjectGenerator.PerPlatformMode is RunTargetPerPlatform)
			List<UnrealTargetPlatform> TargetPlatforms = Platform == null ? XcodeProjectFileGenerator.XcodePlatforms : new() { Platform.Value };
			foreach (UnrealTargetPlatform TargetPlatform in TargetPlatforms)
			{
				XcodeRunTarget RunTarget = new XcodeRunTarget(this, TargetPlatform, BuildTarget);
				RunTargets.Add(RunTarget);
				References.Add(RunTarget);
			}

			ProjectBuildConfigs = new XcodeBuildConfigList(Platform, UnrealData.ProductName, UnrealData.AllConfigs);
			References.Add(ProjectBuildConfigs);

			// create the Projet xcconfig
			CreateXcconfigFile(this, $"{UnrealData.ProductName}_Project");

			// create per-config Xcconfig files
			foreach (XcodeBuildConfig Config in ProjectBuildConfigs.BuildConfigs)
			{
				Config.CreateXcconfigFile(this, $"{UnrealData.ProductName}_{Config.Info.DisplayName.Replace(" ", "")}");
			}


			// make an indexing target if we aren't just a run-only project, and it has buildable source files
			if (!XcodeProjectFileGenerator.bGenerateRunOnlyProject && UnrealData.BatchedFiles.Count != 0)
			{				
				// index isn't a dependency of run, it's simply a target that xcode will find to index from
				XcodeIndexTarget IndexTarget = new XcodeIndexTarget(this);
				References.Add(IndexTarget);
			}
		}

		public override void Write(StringBuilder Content)
		{
			Content.WriteLine("/* Begin PBXProject section */");

			Content.WriteLine(2, $"{Guid} /* Project object */ = {{");
			Content.WriteLine(3,	"isa = PBXProject;");
			Content.WriteLine(3,	"attributes = {");
			Content.WriteLine(4,		"LastUpgradeCheck = 2000;");
			Content.WriteLine(4,		"ORGANIZATIONNAME = \"Epic Games, Inc.\";");
			Content.WriteLine(4,		"TargetAttributes = {");
			foreach (XcodeRunTarget RunTarget in RunTargets)
			{ 
				Content.WriteLine(5,			$"{RunTarget.Guid} = {{");
				Content.WriteLine(6,				$"ProvisioningStyle = {ProvisioningStyle};");
				Content.WriteLine(5,			"};");
			}
			Content.WriteLine(4,		"};");
			Content.WriteLine(3,	"};");
			Content.WriteLine(3,	$"buildConfigurationList = {ProjectBuildConfigs.Guid} /* Build configuration list for PBXProject \"{ProjectBuildConfigs.TargetName}\" */;");
			Content.WriteLine(3,	"compatibilityVersion = \"Xcode 8.0\";");
			Content.WriteLine(3,	"developmentRegion = English;");
			Content.WriteLine(3,	"hasScannedForEncodings = 0;");
			Content.WriteLine(3,	"knownRegions = (");
			Content.WriteLine(4,		"en");
			Content.WriteLine(3,	");");
			Content.WriteLine(3,	$"mainGroup = {FileCollection.MainGroupGuid};");
			Content.WriteLine(3,	$"productRefGroup = {FileCollection.GetProductGroupGuid()};");
			Content.WriteLine(3,	"projectDirPath = \"\";");
			Content.WriteLine(3,	"projectRoot = \"\";");
			Content.WriteLine(3,	"targets = (");
			foreach (XcodeTarget Target in XcodeProjectNode.GetNodesOfType<XcodeTarget>(this))
			{
				Content.WriteLine(4, $"{Target.Guid} /* {Target.Name} */,");
			}
			Content.WriteLine(3, ");");
			Content.WriteLine(2, "};");

			Content.WriteLine("/* End PBXProject section */");
		}

		private List<string> GetSupportedOrientations(ConfigHierarchy Ini)
		{
			List<string> Orientations = new();

			bool bSupported = true;
			if (Ini.TryGetValue("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bSupportsPortraitOrientation", out bSupported) && bSupported)
			{
				Orientations.Add("UIInterfaceOrientationPortrait");
			}
			if (Ini.TryGetValue("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bSupportsUpsideDownOrientation", out bSupported) && bSupported)
			{
				Orientations.Add("UIInterfaceOrientationPortraitUpsideDown");
			}

			string? PreferredLandscapeOrientation;
			Ini.TryGetValue("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "PreferredLandscapeOrientation", out PreferredLandscapeOrientation);
			bool bSupportsLandscapeLeft = false;
			Ini.TryGetValue("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bSupportsLandscapeLeftOrientation", out bSupportsLandscapeLeft);
			bool bSupportsLandscapeRight = false;
			Ini.TryGetValue("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bSupportsLandscapeRightOrientation", out bSupportsLandscapeRight);

			if (bSupportsLandscapeLeft && PreferredLandscapeOrientation == "LandscapeLeft")
			{
				Orientations.Add("UIInterfaceOrientationLandscapeLeft");
			}
			if (bSupportsLandscapeRight)
			{
				Orientations.Add("UIInterfaceOrientationLandscapeRight");
			}
			if (bSupportsLandscapeLeft && PreferredLandscapeOrientation != "LandscapeLeft")
			{
				Orientations.Add("UIInterfaceOrientationLandscapeLeft");
			}

			return Orientations;
		}

		string FindBundleID(FileReference PlistFile)
		{
			string Identifier = XcodeProjectFile.Plist("Print :CFBundleIdentifier", PlistFile.FullName);
			// handle error
			if (Identifier.StartsWith("Print:"))
			{
				return "";
			}
			return Identifier;
		}

		public override void WriteXcconfigFile()
		{
			if (Xcconfig == null)
			{
				return;
			}

			string UEDir = XcodeFileCollection.ConvertPath(Path.GetFullPath(Directory.GetCurrentDirectory() + "../../.."));

			ConfigHierarchy SharedPlatformIni = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, UnrealData.UProjectFileLocation?.Directory, UnrealTargetPlatform.Mac);

			// read some settings from new config settings
			bool bAutomaticSigning = true;
			string? SigningTeam;
			string? SigningPrefix;
			SharedPlatformIni.TryGetValue("XcodeConfiguration", "bUseModernCodeSigning", out bAutomaticSigning);
			SharedPlatformIni.TryGetValue("XcodeConfiguration", "ModernSigningTeam", out SigningTeam);
			SharedPlatformIni.TryGetValue("XcodeConfiguration", "ModernSigningPrefix", out SigningPrefix);

			List<string> SupportedDevices = new();

			StringBuilder Content = new();

			// figure out the directory that contains the Binaries and Intermediate directories
			string ProjectRootDir = (UnrealData.UProjectFileLocation == null) ? $"{UEDir}/Engine" : UnrealData.UProjectFileLocation.Directory.FullName;

			Xcconfig.AppendLine("// Unreal projcet-wide variables");
			Xcconfig.AppendLine($"UE_PRODUCT_NAME = {UnrealData.ProductName}");
			Xcconfig.AppendLine($"UE_PRODUCT_NAME_STRIPPED = {UnrealData.ProductName.Replace("_", "").Replace(" ", "")}");
			Xcconfig.AppendLine($"UE_DISPLAY_NAME = {UnrealData.DisplayName}");
			Xcconfig.AppendLine($"UE_SIGNING_PREFIX = {SigningPrefix}");
			Xcconfig.AppendLine($"UE_ENGINE_DIR = {Unreal.EngineDirectory}");
			Xcconfig.AppendLine("");


			// Info.plist generation settings

			////////////////////////
			// IOS
			////////////////////////

			if (ProjectBuildConfigs.bSupportsIOS)
			{
				string SettingFilter = Platform != null ? "" : "[sdk=iphoneos*]";
				ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, UnrealData.UProjectFileLocation?.Directory, UnrealTargetPlatform.IOS);

				Xcconfig.AppendLine("// IOS Settings");

				SupportedDevices.Add(UnrealData.IOSProjectSettings!.RuntimeDevices);

				Xcconfig.AppendLine($"CONFIGURATION_BUILD_DIR{SettingFilter} = {UnrealData.AllConfigs[0].IOSExecutablePath!.Directory}/Payload");
				Xcconfig.AppendLine($"IPHONEOS_DEPLOYMENT_TARGET = {UnrealData.IOSProjectSettings.RuntimeVersion}");

				if (UnrealData.PlistFiles[MetadataPlatform.IOS].Mode == MetadataMode.UpdateTemplate)
				{
					// allow Xcode to generate the final plist file from our input, some INFOPLIST settings and other settings 
					Xcconfig.AppendLine("GENERATE_INFOPLIST_FILE = YES");
					Xcconfig.AppendLine($"ASSETCATALOG_COMPILER_APPICON_NAME = AppIcon");
					Xcconfig.AppendLine($"CURRENT_PROJECT_VERSION{SettingFilter} = $(UE_IOS_BUILD_VERSION)");

					// short version string
					string BundleShortVersion;
					Ini.GetString("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "VersionInfo", out BundleShortVersion);
					Xcconfig.AppendLine($"MARKETING_VERSION{SettingFilter} = {BundleShortVersion}");
				}


				if (UnrealData.IOSProvisioningData != null && UnrealData.bWriteCodeSigningSettings)
				{
					if (!string.IsNullOrEmpty(UnrealData.IOSProvisioningData.MobileProvisionUUID))
					{
						Xcconfig.AppendLine($"PROVISIONING_PROFILE_SPECIFIER{SettingFilter} = {UnrealData.IOSProvisioningData.MobileProvisionUUID}");
					}
					string IOSCert = UnrealData.IOSProvisioningData.SigningCertificate ?? (UnrealData.bForDistribution ? "Apple Distribution" : "Apple Developer");
					Xcconfig.AppendLine($"CODE_SIGN_IDENTITY{SettingFilter} = {IOSCert}");
				}


				// Info plist generation settings
				//Xcconfig.AppendLine("INFOPLIST_KEY_UIStatusBarHidden = YES");
				//Xcconfig.AppendLine("INFOPLIST_KEY_UIRequiresFullScreen = YES");

				List<string> SupportedOrientations = GetSupportedOrientations(Ini);
				Content.WriteLine(3, $"INFOPLIST_KEY_UISupportedInterfaceOrientations = \"{string.Join(" ", SupportedOrientations)}\"");

				//if (UnrealData.IOSProjectSettings.BundleName != "" && UnrealData.IOSProjectSettings.BundleName != "[PROJECT_NAME]")
				//{
				//	string DisplayName = UnrealData.IOSProjectSettings.BundleDisplayName.Replace("[PROJECT_NAME]", ProjectName).Replace("_", "");
				//	Content.WriteLine(3, $"INFOPLIST_KEY_CFBundleName = {DisplayName}");
				//}

				//if (UnrealData.IOSProjectSettings.BundleDisplayName != "" && UnrealData.IOSProjectSettings.BundleDisplayName != "[PROJECT_NAME]")
				//{
				//	string DisplayName = UnrealData.IOSProjectSettings.BundleDisplayName.Replace("[PROJECT_NAME]", ProjectName).Replace("_", "");
				//	Content.WriteLine(3, $"INFOPLIST_KEY_CFBundleDisplayName = {DisplayName}");
				//}

			}

			////////////////////////
			// TVOS
			////////////////////////

			if (ProjectBuildConfigs.bSupportsTVOS)
			{
				string SettingFilter = Platform != null ? "" : "[sdk=appletvos*]";
				ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, UnrealData.UProjectFileLocation?.Directory, UnrealTargetPlatform.TVOS);

				Xcconfig.AppendLine("");
				Xcconfig.AppendLine("// TVOS Settings");

				SupportedDevices.Add(UnrealData.TVOSProjectSettings!.RuntimeDevices);

				Xcconfig.AppendLine($"CONFIGURATION_BUILD_DIR{SettingFilter} = {UnrealData.AllConfigs[0].TVOSExecutablePath!.Directory}/Payload");
				Xcconfig.AppendLine($"TVOS_DEPLOYMENT_TARGET = {UnrealData.TVOSProjectSettings.RuntimeVersion}");

				if (UnrealData.PlistFiles[MetadataPlatform.IOS].Mode == MetadataMode.UpdateTemplate)
				{
					// allow Xcode to generate the final plist file from our input, some INFOPLIST settings and other settings 
					Xcconfig.AppendLine("GENERATE_INFOPLIST_FILE = YES");
					Xcconfig.AppendLine($"ASSETCATALOG_COMPILER_APPICON_NAME = AppIcon");
					Xcconfig.AppendLine($"CURRENT_PROJECT_VERSION{SettingFilter} = $(UE_TVOS_BUILD_VERSION)");

					// short version string
					string BundleShortVersion;
					Ini.GetString("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "VersionInfo", out BundleShortVersion);
					Xcconfig.AppendLine($"MARKETING_VERSION{SettingFilter} = {BundleShortVersion}");
				}

				if (UnrealData.TVOSProvisioningData != null && UnrealData.bWriteCodeSigningSettings)
				{
					if (!string.IsNullOrEmpty(UnrealData.TVOSProvisioningData.MobileProvisionUUID))
					{
						Xcconfig.AppendLine($"PROVISIONING_PROFILE_SPECIFIER{SettingFilter} = {UnrealData.TVOSProvisioningData.MobileProvisionUUID}");
					}
					string TVOSCert = UnrealData.TVOSProvisioningData.SigningCertificate ?? (UnrealData.bForDistribution ? "Apple Distribution" : "Apple Developer");
					Xcconfig.AppendLine($"CODE_SIGN_IDENTITY{SettingFilter} = {TVOSCert}");
				}


				// Info plist generation settings
				//Xcconfig.AppendLine("INFOPLIST_KEY_UIStatusBarHidden = YES");
				//Xcconfig.AppendLine("INFOPLIST_KEY_UIRequiresFullScreen = YES");
			}

			////////////////////////
			// MAC
			////////////////////////

			if (ProjectBuildConfigs.bSupportsMac)
			{
				string SettingFilter = Platform != null ? "" : "[sdk=macosx*]";

				Xcconfig.AppendLine("");
				Xcconfig.AppendLine("// Mac Settings");
			}


			////////////////////////
			// IOS / TVOS
			////////////////////////


			////////////////////////
			// MAC / IOS / TVOS
			////////////////////////

			Xcconfig.AppendLine("");
			Xcconfig.AppendLine("// Cross-platform settings, and defaults for anything not specified above (Xcode uses first-seen value, not last-seen)");
			Xcconfig.AppendLine("USE_HEADERMAP = NO");
			Xcconfig.AppendLine("ONLY_ACTIVE_ARCH = YES");
			Xcconfig.AppendLine($"TARGETED_DEVICE_FAMILY = {string.Join(",", SupportedDevices)}");


			if (UnrealData.bWriteCodeSigningSettings)
			{
				Xcconfig.AppendLine("CODE_SIGN_STYLE = " + (bAutomaticSigning ? "Automatic" : "Manual"));
				if (!string.IsNullOrEmpty(SigningTeam))
				{
					Xcconfig.AppendLine($"DEVELOPMENT_TEAM = {SigningTeam}");
				}
			}

			//if (UnrealData.EntitlementsLocation != null)
			//{
			//	string RelativeLoc = UnrealData.EntitlementsLocation.MakeRelativeTo(DirectoryReference.Combine(UnrealData.XcodeProjectFileLocation.ParentDirectory!));
			//	Xcconfig.AppendLine($"CODE_SIGN_ENTITLEMENTS = {RelativeLoc}");
			//}

			Xcconfig.Write();

			// Now for each config write out the specific settings

			DirectoryReference? GameDir = UnrealData.UProjectFileLocation?.Directory;
			string? GamePath = GameDir != null ? XcodeFileCollection.ConvertPath(GameDir.FullName) : null;

			// now pull the bundle id's out, as xcode will warn if they don't match
			string MacEditorBundleID = "com.epicgames.UnrealEditor";
			string MacGameBundleID = UnrealData.BundleIdentifier;
			string IOSBundleID = UnrealData.BundleIdentifier;
			if (UnrealData.bSupportsMac)
			{
				if (UnrealData.PlistFiles[MetadataPlatform.MacEditor].Mode == MetadataMode.UsePremade)
				{
					MacEditorBundleID = FindBundleID(UnrealData.PlistFiles[MetadataPlatform.MacEditor].File);
				}
				if (UnrealData.PlistFiles[MetadataPlatform.Mac].Mode == MetadataMode.UsePremade)
				{
					MacGameBundleID = FindBundleID(UnrealData.PlistFiles[MetadataPlatform.Mac].File);
				}
			}
			if ((UnrealData.bSupportsIOS || UnrealData.bSupportsTVOS) && UnrealData.PlistFiles[MetadataPlatform.IOS].Mode == MetadataMode.UsePremade)
			{
				IOSBundleID = FindBundleID(UnrealData.PlistFiles[MetadataPlatform.IOS].File);
			}

			// Get Mac architectures supported by this project
			foreach (UnrealBuildConfig Config in UnrealData.AllConfigs)
			{
				XcodeBuildConfig? MatchedConfig = ProjectBuildConfigs.BuildConfigs.FirstOrDefault(x => x.Info == Config);
				if (MatchedConfig == null || !Config.Supports(Platform))
				{
					continue;
				}

				// hook up the Buildconfig that matches this info to this xcconfig file
				XcconfigFile ConfigXcconfig = MatchedConfig.Xcconfig!;

				ConfigXcconfig.AppendLine("// pull in the shared settings for all configs");
				ConfigXcconfig.AppendLine($"#include \"{UnrealData.ProductName}_Project.xcconfig\"");
				ConfigXcconfig.AppendLine("");

				bool bIsUnrealGame = Config.BuildTarget.Equals("UnrealGame", StringComparison.InvariantCultureIgnoreCase);
				bool bIsUnrealClient = Config.BuildTarget.Equals("UnrealClient", StringComparison.InvariantCultureIgnoreCase);

				// Setup Mac stuff
				FileReference MacExecutablePath = Config.MacExecutablePath!;
				string MacExecutableDir = XcodeFileCollection.ConvertPath(MacExecutablePath.Directory.FullName);
				string MacExecutableFileName = MacExecutablePath.GetFileName();
				string SupportedMacArchitectures = string.Join(" ", UnrealData.GetSupportedMacArchitectures(Config, UnrealData.UProjectFileLocation).Architectures.Select(x => x.AppleName));

				string SupportedPlatforms = "";
				if (Config.ProjectTarget!.TargetRules != null)
				{
					if (Config.bSupportsIOS)
					{
						SupportedPlatforms += "iphoneos iphonesimulator ";
					}
					if (Config.bSupportsTVOS)
					{
						SupportedPlatforms += "appletvos ";
					}
					if (Config.bSupportsMac)
					{
						SupportedPlatforms += "macosx ";
					}
				}
				else
				{
					// @todo when does this case happen?
					throw new BuildException("Project without a target??");
//					SupportedPlatforms = "macosx";
				}


				ConfigXcconfig.AppendLine("// Unreal per-config variables");
				if (Config.bSupportsMac)
				{
					ConfigXcconfig.AppendLine($"UE_MAC_EXECUTABLE_NAME = {MacExecutableFileName}");
				}
				if (Config.bSupportsIOS || Config.bSupportsTVOS)
				{
					ConfigXcconfig.AppendLine($"UE_IOS_EXECUTABLE_NAME = {Config.BuildTarget}");
				}
				ConfigXcconfig.AppendLine($"UE_TARGET_NAME = {Config.BuildTarget}");
				ConfigXcconfig.AppendLine($"UE_TARGET_CONFIG = {Config.BuildConfig}");
				ConfigXcconfig.AppendLine("");

				// debug settings
				if (Config.BuildConfig == UnrealTargetConfiguration.Debug)
				{
					ConfigXcconfig.AppendLine("ENABLE_TESTABILITY = YES");
				}


				// list the supported platforms, which will narrow down what other settings are active
				ConfigXcconfig.AppendLine($"SUPPORTED_PLATFORMS = {SupportedPlatforms}");

				// @otodo move VALID_ARCHS up to Project config once we are always universal
				if (ProjectBuildConfigs.bSupportsMac)
				{
					bool bIsEditor = (Config.ProjectTarget!.TargetRules!.Type == TargetType.Editor);
					Metadata PlistData = UnrealData.PlistFiles[bIsEditor ? MetadataPlatform.MacEditor : MetadataPlatform.Mac];
					Metadata EntitlementsData = UnrealData.EntitlementsFiles[bIsEditor ? MetadataPlatform.MacEditor : MetadataPlatform.Mac];

					string SettingFilter = Platform != null ? "" : "[sdk=macosx*]";

					ConfigXcconfig.AppendLine("");
					ConfigXcconfig.AppendLine("// Mac setup");
					ConfigXcconfig.AppendLine($"VALID_ARCHS{SettingFilter} = {SupportedMacArchitectures}");
					ConfigXcconfig.AppendLine($"CONFIGURATION_BUILD_DIR{SettingFilter} = {MacExecutableDir}");
					ConfigXcconfig.AppendLine($"INFOPLIST_FILE{SettingFilter} = {PlistData.XcodeProjectRelative}");
					ConfigXcconfig.AppendLine($"PRODUCT_BUNDLE_IDENTIFIER{SettingFilter} = " + (bIsEditor ? MacEditorBundleID : MacGameBundleID));

					if (PlistData.Mode == MetadataMode.UpdateTemplate)
					{
						ConfigXcconfig.AppendLine("GENERATE_INFOPLIST_FILE = YES");
						ConfigXcconfig.AppendLine($"ASSETCATALOG_COMPILER_APPICON_NAME = AppIcon");

						ConfigXcconfig.AppendLine($"PRODUCT_NAME{SettingFilter} = {MacExecutableFileName}");
						// @todo: get a version for  games, like IOS has
						ConfigXcconfig.AppendLine($"MARKETING_VERSION{SettingFilter} = {MacToolChain.LoadEngineDisplayVersion()}");
						ConfigXcconfig.AppendLine($"CURRENT_PROJECT_VERSION{SettingFilter} = $(UE_MAC_BUILD_VERSION)");
					}

					if (EntitlementsData.Mode == MetadataMode.UsePremade)
					{
						ConfigXcconfig.AppendLine($"CODE_SIGN_ENTITLEMENTS{SettingFilter} = {EntitlementsData.XcodeProjectRelative}");
					}
				}

				if (ProjectBuildConfigs.bSupportsIOS)
				{
					string SettingFilter = Platform != null ? "" : "[sdk=iphoneos*]";

					ConfigXcconfig.AppendLine("");
					ConfigXcconfig.AppendLine("// IOS setup");
					ConfigXcconfig.AppendLine($"INFOPLIST_FILE{SettingFilter} = {UnrealData.PlistFiles[MetadataPlatform.IOS].XcodeProjectRelative}");
					ConfigXcconfig.AppendLine($"PRODUCT_BUNDLE_IDENTIFIER{SettingFilter} = {IOSBundleID}");
					if (UnrealData.PlistFiles[MetadataPlatform.IOS].Mode == MetadataMode.UpdateTemplate)
					{
						ConfigXcconfig.AppendLine($"PRODUCT_NAME{SettingFilter} = {Config.BuildTarget}"); // @todo: change to Path.GetFileName(Config.IOSExecutablePath) when we stop using payload
					}
					if (UnrealData.EntitlementsFiles[MetadataPlatform.IOS].Mode == MetadataMode.UsePremade)
					{
						ConfigXcconfig.AppendLine($"CODE_SIGN_ENTITLEMENTS{SettingFilter} = {UnrealData.EntitlementsFiles[MetadataPlatform.IOS].XcodeProjectRelative}");
					}
				}
				if (ProjectBuildConfigs.bSupportsTVOS)
				{
					string SettingFilter = Platform != null ? "" : "[sdk=appletvos*]";

					ConfigXcconfig.AppendLine("");
					ConfigXcconfig.AppendLine("// TVOS setup");
					ConfigXcconfig.AppendLine($"INFOPLIST_FILE{SettingFilter} = {UnrealData.PlistFiles[MetadataPlatform.IOS].XcodeProjectRelative}");
					ConfigXcconfig.AppendLine($"PRODUCT_BUNDLE_IDENTIFIER{SettingFilter} = {IOSBundleID}");
					if (UnrealData.EntitlementsFiles[MetadataPlatform.IOS].Mode == MetadataMode.UsePremade)
					{
						ConfigXcconfig.AppendLine($"CODE_SIGN_ENTITLEMENTS{SettingFilter} = {UnrealData.EntitlementsFiles[MetadataPlatform.IOS].XcodeProjectRelative}");
					}
				}

				// @todo this is a reminder of plist settings we are not handling yet
#if false
				//// Prepare a temp Info.plist file so Xcode has some basic info about the target immediately after opening the project.
				//// This is needed for the target to pass the settings validation before code signing. UBT will overwrite this plist file later, with proper contents.
				//if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
				//{
				//	bool bCreateMacInfoPlist = !File.Exists(MacInfoPlistPath);
				//	bool bCreateIOSInfoPlist = !File.Exists(IOSInfoPlistPath) && IOSRunTimeVersion != null;
				//	bool bCreateTVOSInfoPlist = !File.Exists(TVOSInfoPlistPath) && TVOSRunTimeVersion != null;
				//	if (bCreateMacInfoPlist || bCreateIOSInfoPlist || bCreateTVOSInfoPlist)
				//	{
				//		DirectoryReference? ProjectPath = GameDir;
				//		DirectoryReference EngineDir = DirectoryReference.Combine(new DirectoryReference(UEDir), "Engine");
				//		string GameName = Config.BuildTarget;
				//		bool bIsClient = false;
				//		if (ProjectPath == null)
				//		{
				//			ProjectPath = EngineDir;
				//		}
				//		if (bIsUnrealGame)
				//		{
				//			ProjectPath = EngineDir;
				//			GameName = "UnrealGame";
				//			bIsClient = (UnrealData.AppName == "UnrealClient");
				//		}

				//		if (bCreateMacInfoPlist)
				//		{
				//			Directory.CreateDirectory(Path.GetDirectoryName(MacInfoPlistPath)!);
				//			UEDeployMac.GeneratePList(ProjectPath.FullName, bIsUnrealGame, GameName, Config.BuildTarget, EngineDir.FullName, MacExecutableFileName);
				//		}
				//		if (bCreateIOSInfoPlist)
				//		{
				//			// get the receipt
				//			FileReference ReceiptFilename;
				//			if (bIsUnrealGame)
				//			{
				//				ReceiptFilename = TargetReceipt.GetDefaultPath(Unreal.EngineDirectory, "UnrealGame", UnrealTargetPlatform.IOS, Config.BuildConfig, "");
				//			}
				//			else
				//			{
				//				ReceiptFilename = TargetReceipt.GetDefaultPath(ProjectPath, GameName, UnrealTargetPlatform.IOS, Config.BuildConfig, "");
				//			}
				//			Directory.CreateDirectory(Path.GetDirectoryName(IOSInfoPlistPath)!);
				//			bool bSupportPortrait, bSupportLandscape;
				//			TargetReceipt? Receipt;
				//			TargetReceipt.TryRead(ReceiptFilename, out Receipt);
				//			bool bBuildAsFramework = UEDeployIOS.GetCompileAsDll(Receipt);
				//			UEDeployIOS.GenerateIOSPList(UnrealData.UProjectFileLocation, Config.BuildConfig, ProjectPath.FullName, bIsUnrealGame, GameName, bIsClient, Config.BuildTarget, EngineDir.FullName, ProjectPath + "/Binaries/IOS/Payload", null, UnrealData.BundleIdentifier, bBuildAsFramework, UnrealData.Logger!, out bSupportPortrait, out bSupportLandscape);
				//		}
				//		if (bCreateTVOSInfoPlist)
				//		{
				//			Directory.CreateDirectory(Path.GetDirectoryName(TVOSInfoPlistPath)!);
				//			UEDeployTVOS.GenerateTVOSPList(ProjectPath.FullName, bIsUnrealGame, GameName, bIsClient, Config.BuildTarget, EngineDir.FullName, ProjectPath + "/Binaries/TVOS/Payload", null, UnrealData.BundleIdentifier, UnrealData.Logger!);
				//		}
				//	}
				//}
#endif

				ConfigXcconfig.Write();
			}
		}
	}

	class XcodeFileCollection
	{
		class ManualFileReference
		{
			public string Guid = XcodeProjectFileGenerator.MakeXcodeGuid();
			public string RelativePath = "";
			public string FileKey = "";
			public string FileType = "";
			public string SourceTree = "";
			public string GroupName = "";
		}

		// we need relativ paths, so this is the directory that paths are relative to
		private DirectoryReference ProjectDirectory;

		// the location of the uproject file, or the engine directory for non-uproject projects
		public DirectoryReference RootDirectory;

		public string MainGroupGuid = "";
		//public string ProductRefGroupGuid = XcodeProjectFileGenerator.MakeXcodeGuid();

		internal Dictionary<string, XcodeFileGroup> Groups = new();
		internal Dictionary<XcodeSourceFile, FileReference?> BuildableFilesToResponseFile = new();
		internal List<XcodeSourceFile> BuildableResourceFiles = new();
		internal List<XcodeSourceFile> AllFiles = new();

		private List<ManualFileReference> ManualFiles = new();
		private Dictionary<string, string> GuidsForGroups = new();


		public XcodeFileCollection(XcodeProjectFile ProjectFile)
		{
			ProjectDirectory = ProjectFile.ProjectFilePath.Directory;
			RootDirectory = Unreal.EngineDirectory;
			SetUProjectLocation(ProjectFile.UnrealData.UProjectFileLocation);
		}

		public XcodeFileCollection(XcodeFileCollection Other)
		{
			ProjectDirectory = Other.ProjectDirectory;
			RootDirectory = Other.RootDirectory;
			MainGroupGuid = Other.MainGroupGuid;
			AllFiles.AddRange(Other.AllFiles);
			BuildableResourceFiles.AddRange(Other.AllFiles);
			ManualFiles.AddRange(Other.ManualFiles);

			BuildableFilesToResponseFile = new(Other.BuildableFilesToResponseFile);
			GuidsForGroups = new(Other.GuidsForGroups);

			foreach (string Key in Other.Groups.Keys)
			{
				Groups.Add(Key, new XcodeFileGroup(Other.Groups[Key]));
			}

		}

		public void SetUProjectLocation(FileReference? UProjectLocation)
		{
			RootDirectory = UProjectLocation == null || UProjectLocation.ContainsName("Programs", 0) ? (Unreal.EngineDirectory) : UProjectLocation!.Directory;
		}

		public void ProcessFile(XcodeSourceFile File, bool bIsForBuild)
		{
			// remember all buildable files
			if (bIsForBuild)
			{
				string FileExtension = File.Reference.GetExtension();

				if (IsSourceCode(FileExtension))
				{
					// look if it contains any of the exluded names, and is so, don't include it
					if (!ExcludedFolders.Any(x => File.Reference.ContainsName(x, 0)))
					{
						// will fill in the response file later
						BuildableFilesToResponseFile[File] = null;
					}
				}
				else if (IsResourceFile(FileExtension))
				{
					BuildableResourceFiles.Add(File);
				}
			}
			

			// group the files by path
			XcodeFileGroup? Group = FindGroupByAbsolutePath(File.Reference.Directory);
			if (Group != null)
			{
				AllFiles.Add(File);
				Group.Files.Add(File);
			}
			else
			{
				string GroupName = File.Reference.IsUnderDirectory(Unreal.EngineDirectory) ? "EngineReferences" : "ExternalReferences";
				AddFileReference(File.FileRefGuid, File.Reference.FullName, GetFileKey(File.Reference.GetExtension()), GetFileType(File.Reference.GetExtension()), "<absolute>", "EngineReferences");
			}
		}

		/// <summary>
		/// The project needs to point to the product group, so we expose the Guid here
		/// </summary>
		/// <returns></returns>
		public string GetProductGroupGuid()
		{
			return GuidsForGroups["Products"];
		}

		public void AddFileReference(string Guid, string RelativePath, string FileKey, string FileType, string SourceTree/*="SOURCE_ROOT"*/, string GroupName)
		{
			ManualFiles.Add(new ManualFileReference()
			{
				Guid = Guid,
				RelativePath = RelativePath,
				FileKey = FileKey,
				FileType = FileType,
				SourceTree = SourceTree.StartsWith("<") ? $"\"{SourceTree}\"" : SourceTree,
				GroupName = GroupName,
			});

			// make sure each unique group has a guid
			if (!GuidsForGroups.ContainsKey(GroupName))
			{
				GuidsForGroups[GroupName] = XcodeProjectFileGenerator.MakeXcodeGuid();
			}
		}


		public void Write(StringBuilder Content)
		{
			Content.WriteLine("/* Begin PBXBuildFile section */");
			foreach (KeyValuePair<XcodeSourceFile, FileReference?> Pair in BuildableFilesToResponseFile)
			{
				string CompileFlags = "";
				if (Pair.Value != null)
				{
					CompileFlags = $" settings = {{COMPILER_FLAGS = \"@{Pair.Value.FullName}\"; }};";
				}
				Content.WriteLine($"\t\t{Pair.Key.FileGuid} = {{isa = PBXBuildFile; fileRef = {Pair.Key.FileRefGuid};{CompileFlags} }}; /* {Pair.Key.Reference.GetFileName()} */");
			}
			foreach (XcodeSourceFile Resource in BuildableResourceFiles)
			{
				Content.WriteLine($"\t\t{Resource.FileGuid} = {{isa = PBXBuildFile; fileRef = {Resource.FileRefGuid}; }}; /* {Resource.Reference.GetFileName()} */");
			}
			Content.WriteLine("/* End PBXBuildFile section */");
			Content.WriteLine();


			Content.WriteLine("/* Begin PBXFileReference section */");
			foreach (XcodeSourceFile File in AllFiles)
			{
				string FileName = File.Reference.GetFileName();
				string FileExtension = Path.GetExtension(FileName);
				string FileType = GetFileType(FileExtension);
				string FileKey = GetFileKey(FileExtension);
				string RelativePath = Utils.CleanDirectorySeparators(File.Reference.MakeRelativeTo(ProjectDirectory), '/');
				string SourceTree = "SOURCE_ROOT";

				Content.WriteLine($"\t\t{File.FileRefGuid} = {{isa = PBXFileReference; {FileKey} = {FileType}; name = \"{FileName}\"; path = \"{RelativePath}\"; sourceTree = {SourceTree}; }};");
			}
			foreach (ManualFileReference File in ManualFiles)
			{
				string FileName = Path.GetFileName(File.RelativePath);
				Content.WriteLine($"\t\t{File.Guid} = {{isa = PBXFileReference; {File.FileKey} = {File.FileType}; name = \"{FileName}\"; path = \"{File.RelativePath}\"; sourceTree = {File.SourceTree}; }};");
			}
			Content.WriteLine("/* End PBXFileReference section */");
			Content.WriteLine();

			WriteGroups(Content);
		}

		private void WriteGroups(StringBuilder Content)
		{
			XcodeFileGroup? RootGroup = FindRootFileGroup(Groups);
			if (RootGroup == null)
			{
				return;
			}

			string XcconfigsRefGroupGuid = XcodeProjectFileGenerator.MakeXcodeGuid();

			Content.WriteLine("/* Begin PBXGroup section */");


			// write main/root group and it's children (adding the manual groups to the main group, which is the true param here0
			WriteGroup(Content, RootGroup, bIsRootGroup: true);

			// write some manual groups
			foreach (var Pair in GuidsForGroups)
			{
				Content.WriteLine($"\t\t{Pair.Value} /* {Pair.Key} */ = {{");
				Content.WriteLine("\t\t\tisa = PBXGroup;");
				Content.WriteLine("\t\t\tchildren = (");
				foreach (ManualFileReference Ref in ManualFiles.Where(x => x.GroupName == Pair.Key))
				{
					Content.WriteLine($"\t\t\t\t{Ref.Guid} /* {Path.GetFileName(Ref.RelativePath)} */,");
				}
				Content.WriteLine("\t\t\t);");
				Content.WriteLine($"\t\t\tname = {Pair.Key};");
				Content.WriteLine("\t\t\tsourceTree = \"<group>\";");
				Content.WriteLine("\t\t};");

			}

			Content.WriteLine("/* End PBXGroup section */");
		}


		private void WriteGroup(StringBuilder Content, XcodeFileGroup Group, bool bIsRootGroup)
		{
			if (!Group.bIsReference)
			{
				Content.WriteLine($"\t\t{Group.GroupGuid} = {{");
				Content.WriteLine("\t\t\tisa = PBXGroup;");
				Content.WriteLine("\t\t\tchildren = (");
				foreach (XcodeFileGroup ChildGroup in Group.Children.Values)
				{
					Content.WriteLine($"\t\t\t\t{ChildGroup.GroupGuid} /* {ChildGroup.GroupName} */,");
				}
				foreach (XcodeSourceFile File in Group.Files)
				{
					Content.WriteLine($"\t\t\t\t{File.FileRefGuid} /* {File.Reference.GetFileName()} */,");
				}

				if (bIsRootGroup)
				{
					foreach (var Pair in GuidsForGroups)
					{
						Content.WriteLine($"\t\t\t\t{Pair.Value} /* {Pair.Key} */,");
					}
				}
				Content.WriteLine("\t\t\t);");
				if (!bIsRootGroup)
				{
					Content.WriteLine("\t\t\tname = \"" + Group.GroupName + "\";");
					Content.WriteLine("\t\t\tpath = \"" + Group.GroupPath + "\";");
					Content.WriteLine("\t\t\tsourceTree = \"<absolute>\";");
				}
				else
				{
					Content.WriteLine("\t\t\tsourceTree = \"<group>\";");
				}
				Content.WriteLine("\t\t};");

				foreach (XcodeFileGroup ChildGroup in Group.Children.Values)
				{
					WriteGroup(Content, ChildGroup, bIsRootGroup: false);
				}
			}
		}

		public static string GetRootGroupGuid(Dictionary<string, XcodeFileGroup> GroupsDict)
		{
			return FindRootFileGroup(GroupsDict)!.GroupGuid;
		}

		private static XcodeFileGroup? FindRootFileGroup(Dictionary<string, XcodeFileGroup> GroupsDict)
		{
			foreach (XcodeFileGroup Group in GroupsDict.Values)
			{
				if (Group.Children.Count > 1 || Group.Files.Count > 0)
				{
					return Group;
				}
				else
				{
					XcodeFileGroup? Found = FindRootFileGroup(Group.Children);
					if (Found != null)
					{
						return Found;
					}
				}
			}
			return null;
		}


		/// <summary>
		/// Gets Xcode file category based on its extension
		/// </summary>
		internal static string GetFileCategory(string Extension)
		{
			// @todo Mac: Handle more categories
			switch (Extension)
			{
				case ".framework":
					return "Frameworks";
				default:
					return "Sources";
			}
		}

		/// <summary>
		/// Gets Xcode file type based on its extension
		/// </summary>
		internal static string GetFileType(string Extension)
		{
			// @todo Mac: Handle more file types
			switch (Extension)
			{
				case ".c":
				case ".m":
					return "sourcecode.c.objc";
				case ".cc":
				case ".cpp":
				case ".mm":
					return "sourcecode.cpp.objcpp";
				case ".h":
				case ".inl":
				case ".pch":
					return "sourcecode.c.h";
				case ".framework":
					return "wrapper.framework";
				case ".plist":
					return "text.plist.xml";
				case ".png":
					return "image.png";
				case ".icns":
					return "image.icns";
				case ".xcassets":
					return "folder.assetcatalog";
				case ".storyboardc":
					return "wrapper.storyboardc";
				case ".storyboard":
					return "file.storyboard";
				default:
					return "file.text";
			}
		}

		// most types Xcode wants as explicitFileType, but some want lastKnownFileType
		internal static string GetFileKey(string Extension)
		{
			switch (Extension)
			{
				case ".storyboard":
					return "lastKnownFileType";
				default:
					return "explicitFileType";
			}
		}
		/// <summary>
		/// Returns true if Extension is a known extension for files containing source code
		/// </summary>
		internal static bool IsSourceCode(string Extension)
		{
			return Extension == ".c" || Extension == ".cc" || Extension == ".cpp" || Extension == ".m" || Extension == ".mm";
		}

		/// <summary>
		/// Returns true if Extension is a known extension for files containing resource data
		/// </summary>
		internal static bool IsResourceFile(string Extension)
		{
			return Extension == ".xcassets" || Extension == ".storyboardc" || Extension == ".storyboard";
		}

		/// <summary>
		/// Cache the folders excluded by Mac
		/// </summary>
		static string[] ExcludedFolders = PlatformExports.GetExcludedFolderNames(UnrealTargetPlatform.Mac);

		/// <summary>
		/// Returns a project navigator group to which the file should belong based on its path.
		/// Creates a group tree if it doesn't exist yet.
		/// </summary>
		internal XcodeFileGroup? FindGroupByAbsolutePath(DirectoryReference DirectoryPath)
		{
			string AbsolutePath = DirectoryPath.FullName;
			string[] Parts = AbsolutePath.Split(Path.DirectorySeparatorChar);
			string CurrentPath = "/";
			Dictionary<string, XcodeFileGroup> CurrentSubGroups = Groups;

			for (int Index = 1; Index < Parts.Count(); ++Index)
			{
				string Part = Parts[Index];

				if (CurrentPath.Length > 1)
				{
					CurrentPath += Path.DirectorySeparatorChar;
				}

				CurrentPath += Part;

				XcodeFileGroup CurrentGroup;
				if (!CurrentSubGroups.ContainsKey(CurrentPath))
				{
					// we only want files under the project/engine dir, we have to make external references to other files, otherwise the grouping gets all broken
					if (!DirectoryPath.IsUnderDirectory(RootDirectory))
					{
						return null;
					}


					CurrentGroup = new XcodeFileGroup(Path.GetFileName(CurrentPath), CurrentPath, GetFileType(Path.GetExtension(CurrentPath)).StartsWith("folder"));
					if (Groups.Count == 0)
					{
						MainGroupGuid = CurrentGroup.GroupGuid;
					}
					CurrentSubGroups.Add(CurrentPath, CurrentGroup);
				}
				else
				{
					CurrentGroup = CurrentSubGroups[CurrentPath];
				}

				if (CurrentPath == AbsolutePath)
				{
					return CurrentGroup;
				}

				CurrentSubGroups = CurrentGroup.Children;
			}

			return null;
		}

		/// <summary>
		/// Convert all paths to Apple/Unix format (with forward slashes)
		/// </summary>
		/// <param name="InPath">The path to convert</param>
		/// <returns>The normalized path</returns>
		internal static string ConvertPath(string InPath)
		{
			return InPath.Replace("\\", "/");
		}
	}


	class XcodeProjectFile : ProjectFile
	{

		/// <summary>
		/// Constructs a new project file object
		/// </summary>
		/// <param name="InitFilePath">The path to the project file on disk</param>
		/// <param name="BaseDir">The base directory for files within this project</param>
		/// <param name="bIsForDistribution">True for distribution builds</param>
		/// <param name="BundleID">Override option for bundle identifier</param>
		/// <param name="AppName"></param>
		public XcodeProjectFile(FileReference InitFilePath, DirectoryReference BaseDir, bool bIsForDistribution, string BundleID, string AppName)
			: base(InitFilePath, BaseDir)
		{
			UnrealData = new UnrealData(InitFilePath, bIsForDistribution, BundleID, AppName);

			// create the container for all the files that will 
			SharedFileCollection = new XcodeFileCollection(this);
			FileCollection = SharedFileCollection;
		}

		public UnrealData UnrealData;

		/// <summary>
		///  The PBXPRoject node, root of everything
		/// </summary>
		public Dictionary<XcodeProject, UnrealTargetPlatform?> RootProjects = new();


		private XcodeProjectLegacy.XcodeProjectFile? LegacyProjectFile = null;
		private bool bHasCheckedForLegacy = false;
		public bool bHasLegacyProject => LegacyProjectFile != null;

		/// <summary>
		/// Gathers the files and generates project sections
		/// </summary>
		private XcodeFileCollection SharedFileCollection;
		public XcodeFileCollection FileCollection;


		/// <summary>
		/// Allocates a generator-specific source file object
		/// </summary>
		/// <param name="InitFilePath">Path to the source file on disk</param>
		/// <param name="InitProjectSubFolder">Optional sub-folder to put the file in.  If empty, this will be determined automatically from the file's path relative to the project file</param>
		/// <returns>The newly allocated source file object</returns>
		public override SourceFile? AllocSourceFile(FileReference InitFilePath, DirectoryReference? InitProjectSubFolder)
		{
			if (InitFilePath.GetFileName().StartsWith("."))
			{
				return null;
			}
			return new XcodeSourceFile(InitFilePath, InitProjectSubFolder);
		}

		private void ConditionalCreateLegacyProject()
		{
			if (!bHasCheckedForLegacy)
			{
				bHasCheckedForLegacy = true;
				if (ProjectTargets.Count == 0)
				{
					throw new BuildException("Expected to have targets before AddModule is called");
				}
				FileReference? UProjectFileLocation = UnrealData.FindUProjectFileLocation(this);
				ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, UProjectFileLocation?.Directory, UnrealTargetPlatform.Mac);
				bool bUseModernXcode;
				if (!(Ini.TryGetValue("XcodeConfiguration", "bUseModernXcode", out bUseModernXcode) && bUseModernXcode))
				{
					LegacyProjectFile = new XcodeProjectLegacy.XcodeProjectFile(ProjectFilePath, BaseDir, UnrealData.bForDistribution, UnrealData.BundleIdentifier, UnrealData.AppName);
					LegacyProjectFile.ProjectTargets.AddRange(ProjectTargets);
					LegacyProjectFile.SourceFiles.AddRange(SourceFiles);
					LegacyProjectFile.IsGeneratedProject = IsGeneratedProject;
					LegacyProjectFile.IsStubProject = IsStubProject;
					LegacyProjectFile.IsForeignProject = IsForeignProject;
				}
			}
		}

		public override void AddModule(UEBuildModuleCPP Module, CppCompileEnvironment CompileEnvironment)
		{
			ConditionalCreateLegacyProject();

			if (LegacyProjectFile != null)
			{
				LegacyProjectFile.AddModule(Module, CompileEnvironment);
				return;
			}

			UnrealData.AddModule(Module, CompileEnvironment);
		}

		/// <summary>
		/// Generates bodies of all sections that contain a list of source files plus a dictionary of project navigator groups.
		/// </summary>
		private void ProcessSourceFiles()
		{
			// process the files that came from UE/cross-platform land
			SourceFiles.SortBy(x => x.Reference.FullName);

			foreach (XcodeSourceFile SourceFile in SourceFiles.OfType<XcodeSourceFile>())
			{
				SharedFileCollection.ProcessFile(SourceFile, bIsForBuild:IsGeneratedProject);
			}

			// cache the main group
			SharedFileCollection.MainGroupGuid = XcodeFileCollection.GetRootGroupGuid(SharedFileCollection.Groups);

			// filter each file into the appropriate batch
			foreach (XcodeSourceFile File in SharedFileCollection.BuildableFilesToResponseFile.Keys)
			{
				AddFileToBatch(File, SharedFileCollection);
			}

			// write out the response files for each batch now that everything is done
			foreach (UnrealBatchedFiles Batch in UnrealData.BatchedFiles)
			{
				Batch.GenerateResponseFile();
			}
		}

		private void AddFileToBatch(XcodeSourceFile File, XcodeFileCollection FileCollection)
		{
			foreach (UnrealBatchedFiles Batch in UnrealData.BatchedFiles)
			{
				foreach (UEBuildModuleCPP Module in Batch.Modules)
				{
					if (Module.ContainsFile(File.Reference))
					{
						Batch.Files.Add(File);
						FileCollection.BuildableFilesToResponseFile[File] = Batch.ResponseFile;
						return;
					}
				}
			}
		}


		// null platform means use the old way, with multiple platforms per project
		private string Suffix(UnrealTargetPlatform? Platform)
		{
			return Platform == null ? "" : $" ({Platform})";
		}

		public DirectoryReference ProjectDirPathForPlatform(UnrealTargetPlatform? Platform)
		{
			return DirectoryReference.Combine(ProjectFilePath.Directory, $"{ProjectFilePath.GetFileNameWithoutExtension()}{Suffix(Platform)}{ProjectFilePath.GetExtension()}");
		}

		public FileReference ProjectFilePathForPlatform(UnrealTargetPlatform? Platform)
		{
			return new FileReference(ProjectDirPathForPlatform(Platform).FullName);
		}

		public FileReference PBXFilePathForPlatform(UnrealTargetPlatform? Platform)
		{
			return FileReference.Combine(ProjectDirPathForPlatform(Platform), "project.pbxproj");
		}

		/// Implements Project interface
		public override bool WriteProjectFile(List<UnrealTargetPlatform> InPlatforms, List<UnrealTargetConfiguration> InConfigurations, PlatformProjectGeneratorCollection PlatformProjectGenerators, ILogger Logger)
		{
			ConditionalCreateLegacyProject();

			if (LegacyProjectFile != null)
			{
				return LegacyProjectFile.WriteProjectFile(InPlatforms, InConfigurations, PlatformProjectGenerators, Logger);
			}

			if (UnrealData.Initialize(this, InConfigurations, Logger) == false)
			{
				// if we failed to initialize, we silently return to move on (it's not an error, it's a project with nothing to do)
				return true;
			}

			// look for an existing project to use as a template (if none found, create one from scratch)
			DirectoryReference BuildDirLocation = UnrealData.UProjectFileLocation == null ? Unreal.EngineDirectory : UnrealData.UProjectFileLocation.Directory;
			string ExistingProjectName = UnrealData.ProductName;
			FileReference TemplateProject = FileReference.Combine(BuildDirLocation, $"Build/IOS/{UnrealData.ProductName}.xcodeproj/project.pbxproj");

			// @todo this for per-platform!
			UnrealData.bIsMergingProjects = FileReference.Exists(TemplateProject);
			UnrealData.bWriteCodeSigningSettings = !UnrealData.bIsMergingProjects;


			// turn all UE files into internal representation
			ProcessSourceFiles();

			bool bSuccess = true;
			foreach (UnrealTargetPlatform? Platform in XcodeProjectFileGenerator.WorkspacePlatforms)
			{
				// skip the platform if the project has no configurations for it
				if (!UnrealData.AllConfigs.Any(x => x.Supports(Platform)))
				{
					continue;
				}
				FileReference PBXFilePath = PBXFilePathForPlatform(Platform);

				// now create the xcodeproject elements (project -> target -> buildconfigs, etc)
				FileCollection = new XcodeFileCollection(SharedFileCollection);
				XcodeProject RootProject = new XcodeProject(Platform, UnrealData, FileCollection);
				RootProjects[RootProject] = Platform;

				if (FileReference.Exists(TemplateProject))
				{
					// @todo hahahaah
					continue;
					//bSuccess = MergeIntoTemplateProject(PBXFilePath, RootProject, TemplateProject);
				}
				else
				{
					// write metadata now so we can add them to the FileCollection
					ConditionalWriteMetadataFiles(UnrealTargetPlatform.IOS);


					StringBuilder Content = new StringBuilder();

					Content.WriteLine(0, "// !$*UTF8*$!");
					Content.WriteLine(0, "{");
					Content.WriteLine(1, "archiveVersion = 1;");
					Content.WriteLine(1, "classes = {");
					Content.WriteLine(1, "};");
					Content.WriteLine(1, "objectVersion = 46;");
					Content.WriteLine(1, "objects = {");

					// write out the list of files and groups
					FileCollection.Write(Content);

					// now write out the project node and its recursive dependent nodes
					XcodeProjectNode.WriteNodeAndReferences(Content, RootProject);

					Content.WriteLine(1, "};");
					Content.WriteLine(1, $"rootObject = {RootProject.Guid} /* Project object */;");
					Content.WriteLine(0, "}");

					// finally write out the pbxproj file!
					bSuccess = ProjectFileGenerator.WriteFileIfChanged(PBXFilePath.FullName, Content.ToString(), Logger, new UTF8Encoding()) && bSuccess;
				}


				bool bNeedScheme = UnrealData.CanBuildProjectLocally(this, Logger);
				if (bNeedScheme)
				{
					if (bSuccess)
					{
						string ProjectName = ProjectFilePathForPlatform(Platform).GetFileNameWithoutAnyExtensions();
						string? BuildTargetGuid = XcodeProjectNode.GetNodesOfType<XcodeBuildTarget>(RootProject).FirstOrDefault()?.Guid;
						string? IndexTargetGuid = XcodeProjectNode.GetNodesOfType<XcodeIndexTarget>(RootProject).FirstOrDefault()?.Guid;
						WriteSchemeFile(Platform, ProjectName, RootProject.RunTargets, BuildTargetGuid, IndexTargetGuid, UnrealData.bHasEditorConfiguration, UnrealData.UProjectFileLocation != null ? UnrealData.UProjectFileLocation.FullName : "");
					}
				}
				else
				{
					// clean this up because we don't want it persisting if we narrow our project list
					DirectoryReference SchemeDir = GetProjectSchemeDirectory(Platform);

					if (DirectoryReference.Exists(SchemeDir))
					{
						DirectoryReference.Delete(SchemeDir, true);
					}
				}
			}
			return bSuccess;
		}

		private UnrealTargetPlatform CurrentPlistPlatform;
		private void ConditionalWriteMetadataFiles(UnrealTargetPlatform Platform)
		{
			CurrentPlistPlatform = Platform;

			// we now use templates or premade, no writing out here
			foreach (Metadata Data in UnrealData.PlistFiles.Values)
			{
				FileCollection.AddFileReference(XcodeProjectFileGenerator.MakeXcodeGuid(), Data.XcodeProjectRelative, "explicitFileType", "text.plist", "\"<group>\"", "Metadata");
			}
			foreach (Metadata Data in UnrealData.EntitlementsFiles.Values)
			{
				if (Data.Mode == MetadataMode.UsePremade)
				{
					FileCollection.AddFileReference(XcodeProjectFileGenerator.MakeXcodeGuid(), Data.XcodeProjectRelative, "explicitFileType", "text.plist", "\"<group>\"", "Metadata");
				}
			}
#if false
			ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, UnrealData.UProjectFileLocation?.Directory, Platform);

			/////////////////////
			// Entitlements
			/////////////////////

			FileReference? ExistingEntitlements = UnrealData.UProjectFileLocation == null ? null :
				FileReference.Combine(UnrealData.UProjectFileLocation.Directory, $"Build/Xcode/{UnrealData.ProductName}.entitlements");

			if (ExistingEntitlements != null && FileReference.Exists(ExistingEntitlements))
			{
				UnrealData.EntitlementsLocation = ExistingEntitlements;
				UnrealData.bEntitlementsWasPremade = true;
				string RelativeLoc = UnrealData.EntitlementsLocation.MakeRelativeTo(UnrealData.XcodeProjectFileLocation.ParentDirectory!);
				FileCollection.AddFileReference(XcodeProjectFileGenerator.MakeXcodeGuid(), RelativeLoc, "text.plist", "\"<group>\"", "Metadata");
			}
			else
			{
				UnrealData.EntitlementsLocation = FileReference.Combine(UnrealData.XcodeProjectFileLocation.ParentDirectory!, "Metadata", $"{UnrealData.ProductName}.entitlements");
				FileCollection.AddFileReference(XcodeProjectFileGenerator.MakeXcodeGuid(), $"Metadata/{UnrealData.EntitlementsLocation.GetFileName()}", "text.plist.entitlements", "\"<group>\"", "Metadata");

				string Filename = UnrealData.EntitlementsLocation.FullName;
				DirectoryReference.CreateDirectory(UnrealData.EntitlementsLocation.Directory);

				// reset file to start (or create it)
				Plist("Clear", Filename);

				bool bValue = false;
				// remote push notifications
				if (Ini.GetBool("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bEnableRemoteNotificationsSupport", out bValue) && bValue)
				{
					// Xcode will replace this with production when using a distro cert
					Plist("Add aps-environment string development", Filename);
				}

				// Sign in with Apple
				if (Ini.GetBool("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bEnableSignInWithAppleSupport", out bValue) && bValue)
				{
					Plist("Add com.apple.developer.applesignin string Default", Filename);
				}

				// Add Multi-user support for tvOS
				if (Ini.GetBool("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bRunAsCurrentUser", out bValue) && bValue)
				{
					Plist("Add com.apple.developer.user-management array", Filename);
					Plist("Add com.apple.developer.user-management:0 string runs-as-current-user", Filename);
				}
			}
#endif





#if false
			/////////////////////
			// Info.plist
			/////////////////////

			FileReference? ExistingInfoPlist = UnrealData.UProjectFileLocation == null ? null :
				FileReference.Combine(UnrealData.UProjectFileLocation.Directory, $"Build/Xcode/{UnrealData.ProductName}-Info.plist");
			if (ExistingInfoPlist != null && FileReference.Exists(ExistingInfoPlist))
			{
				UnrealData.InfoPlistLocation = ExistingInfoPlist;
				UnrealData.bInfoPlistWasPremade = true;
				string RelativeLoc = UnrealData.InfoPlistLocation.MakeRelativeTo(UnrealData.XcodeProjectFileLocation.ParentDirectory!);
				FileCollection.AddFileReference(XcodeProjectFileGenerator.MakeXcodeGuid(), RelativeLoc, "text.plist", "\"<group>\"", "Metadata");
			}
			else
			{
				UnrealData.InfoPlistLocation = FileReference.Combine(UnrealData.XcodeProjectFileLocation.ParentDirectory!, "Metadata", $"{UnrealData.ProductName}-Info.plist");
				FileCollection.AddFileReference(XcodeProjectFileGenerator.MakeXcodeGuid(), $"Metadata/{UnrealData.InfoPlistLocation.GetFileName()}", "text.plist", "\"<group>\"", "Metadata");


				string Filename = UnrealData.InfoPlistLocation.FullName;

				// reset file to start (or create it)
				Plist("Clear", Filename);

				////////////////////////
				// PLIST MAC / IOS
				////////////////////////


				////////////////////////
				// PLIST MAC
				////////////////////////

				// disable non-exempt encryption
				Plist("Add :ITSAppUsesNonExemptEncryption bool false", Filename);

				////////////////////////
				// PLIST IOS
				////////////////////////

				if (UnrealData.IOSProjectSettings != null)
				{
					bool HACK_bSupportsFilesApp = true;
					if (UnrealData.IOSProjectSettings.bFileSharingEnabled)
					{
						Plist("Add :UIFileSharingEnabled bool true", Filename);
					}
					if (HACK_bSupportsFilesApp)
					{
						Plist("Add :LSSupportsOpeningDocumentsInPlace bool true", Filename);
					}
					Plist("Add :UIViewControllerBasedStatusBarAppearance bool false", Filename);
				}
			}

#endif



#if false

			// in CFBundleURLSchemes array
			if (bEnableFacebookSupport)
			{
				// This is needed for facebook login to redirect back to the app after completion.
				Text.AppendLine(string.Format("\t\t\t\t<string>fb{0}</string>", FacebookAppID));
			}
			if (bEnableGoogleSupport)
			{
				Text.AppendLine(string.Format("\t\t\t\t<string>{0}</string>", GoogleReversedClientId));
			}

			Text.AppendLine("\t<key>CFBundleVersion</key>");
			Text.AppendLine(string.Format("\t<string>{0}</string>", VersionUtilities.UpdateBundleVersion(OldPListData, InEngineDir)));
			Text.AppendLine("\t<key>CFBundleShortVersionString</key>");
			Text.AppendLine(string.Format("\t<string>{0}</string>", BundleShortVersion));


#endif


			// deleted
#if false

			Text.AppendLine("\t<key>CFBundleDevelopmentRegion</key>");
			Text.AppendLine("\t<string>English</string>");

			Text.AppendLine("\t<key>CFBundleExecutable</key>");
			string BundleExecutable = bIsUnrealGame ?
				(bIsClient ? "UnrealClient" : "UnrealGame") :
				(bIsClient ? GameName + "Client" : GameName);
			Text.AppendLine(string.Format("\t<string>{0}</string>", BundleExecutable));

 
#endif

			// moved into .plist
#if false
			Text.AppendLine("\t\t\t<key>CFBundleURLName</key>");
			Text.AppendLine("\t\t\t<string>com.Epic.Unreal</string>");
			Text.AppendLine("\t\t\t<key>CFBundleURLSchemes</key>");
			Text.AppendLine("\t\t\t<array>");
			Text.AppendLine(string.Format("\t\t\t\t<string>{0}</string>", bIsUnrealGame ? "UnrealGame" : GameName));
			Text.AppendLine("\t\t\t</array>");
			Text.AppendLine("\t\t</dict>");
			Text.AppendLine("\t</array>");


			Text.AppendLine("\t<key>UIRequiredDeviceCapabilities</key>");
			Text.AppendLine("\t<array>");
			foreach(string Line in RequiredCaps.Split("\r\n".ToCharArray()))
			{
				if (!string.IsNullOrWhiteSpace(Line))
				{
					Text.AppendLine(Line);
				}
			}
			Text.AppendLine("\t</array>");

			Text.AppendLine("\t<key>CFBundleIcons</key>");
			Text.AppendLine("\t<dict>");
			Text.AppendLine("\t\t<key>CFBundlePrimaryIcon</key>");
			Text.AppendLine("\t\t<dict>");
			Text.AppendLine("\t\t\t<key>CFBundleIconFiles</key>");
			Text.AppendLine("\t\t\t<array>");
			Text.AppendLine("\t\t\t\t<string>AppIcon60x60</string>");
			Text.AppendLine("\t\t\t</array>");
			Text.AppendLine("\t\t\t<key>CFBundleIconName</key>");
			Text.AppendLine("\t\t\t<string>AppIcon</string>");
			Text.AppendLine("\t\t\t<key>UIPrerenderedIcon</key>");
			Text.AppendLine("\t\t\t<true/>");
			Text.AppendLine("\t\t</dict>");
			Text.AppendLine("\t</dict>");
			Text.AppendLine("\t<key>CFBundleIcons~ipad</key>");
			Text.AppendLine("\t<dict>");
			Text.AppendLine("\t\t<key>CFBundlePrimaryIcon</key>");
			Text.AppendLine("\t\t<dict>");
			Text.AppendLine("\t\t\t<key>CFBundleIconFiles</key>");
			Text.AppendLine("\t\t\t<array>");
			Text.AppendLine("\t\t\t\t<string>AppIcon60x60</string>");
			Text.AppendLine("\t\t\t\t<string>AppIcon76x76</string>");
			Text.AppendLine("\t\t\t</array>");
			Text.AppendLine("\t\t\t<key>CFBundleIconName</key>");
			Text.AppendLine("\t\t\t<string>AppIcon</string>");
			Text.AppendLine("\t\t\t<key>UIPrerenderedIcon</key>");
			Text.AppendLine("\t\t\t<true/>");
			Text.AppendLine("\t\t</dict>");
			Text.AppendLine("\t</dict>");
			Text.AppendLine("\t<key>UILaunchStoryboardName</key>");
			Text.AppendLine("\t<string>LaunchScreen</string>");

			if (File.Exists(DirectoryReference.FromFile(ProjectFile) + "/Build/IOS/Resources/Interface/LaunchScreen.storyboard") && VersionUtilities.bCustomLaunchscreenStoryboard)
			{
				string LaunchStoryboard = DirectoryReference.FromFile(ProjectFile) + "/Build/IOS/Resources/Interface/LaunchScreen.storyboard";

				if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
				{
					string outputStoryboard = LaunchStoryboard + "c";
					string argsStoryboard = "--compile " + outputStoryboard + " " + LaunchStoryboard;
					string stdOutLaunchScreen = Utils.RunLocalProcessAndReturnStdOut("ibtool", argsStoryboard, Logger);

					Logger.LogInformation("LaunchScreen Storyboard compilation results : {Results}", stdOutLaunchScreen);
				}
				else
				{
					Logger.LogWarning("Custom Launchscreen compilation storyboard only compatible on Mac for now");
				}
			}


			// add location services descriptions if used
			if (!string.IsNullOrWhiteSpace(LocationAlwaysUsageDescription))
			{
				Text.AppendLine("\t<key>NSLocationAlwaysAndWhenInUseUsageDescription</key>");
				Text.AppendLine(string.Format("\t<string>{0}</string>", LocationAlwaysUsageDescription));
			}
			if (!string.IsNullOrWhiteSpace(LocationWhenInUseDescription))
			{
				Text.AppendLine("\t<key>NSLocationWhenInUseUsageDescription</key>");
				Text.AppendLine(string.Format("\t<string>{0}</string>", LocationWhenInUseDescription));
			}
			// disable HTTPS requirement
			if (bDisableHTTPS)
			{
				Text.AppendLine("\t<key>NSAppTransportSecurity</key>");
				Text.AppendLine("\t\t<dict>");
				Text.AppendLine("\t\t\t<key>NSAllowsArbitraryLoads</key><true/>");
				Text.AppendLine("\t\t</dict>");
			}

			if (bEnableFacebookSupport)
			{
				Text.AppendLine("\t<key>FacebookAppID</key>");
				Text.AppendLine(string.Format("\t<string>{0}</string>", FacebookAppID));
				Text.AppendLine("\t<key>FacebookDisplayName</key>");
				Text.AppendLine(string.Format("\t<string>{0}</string>", FacebookDisplayName));
				
				if (!bEnableAutomaticLogging)
				{
					Text.AppendLine("<key>FacebookAutoLogAppEventsEnabled</key><false/>");
				}
				
				if (!bEnableAdvertisingId)
				{
					Text.AppendLine("<key>FacebookAdvertiserIDCollectionEnabled</key><false/>");
				}

				Text.AppendLine("\t<key>LSApplicationQueriesSchemes</key>");
				Text.AppendLine("\t<array>");
				Text.AppendLine("\t\t<string>fbapi</string>");
				Text.AppendLine("\t\t<string>fb-messenger-api</string>");
				Text.AppendLine("\t\t<string>fb-messenger-share-api</string>");
				Text.AppendLine("\t\t<string>fbauth2</string>");
				Text.AppendLine("\t\t<string>fbshareextension</string>");
				Text.AppendLine("\t</array>");
			}

			if (!string.IsNullOrEmpty(ExtraData))
			{
				ExtraData = ExtraData.Replace("\\n", "\n");
				foreach(string Line in ExtraData.Split("\r\n".ToCharArray()))
				{
					if (!string.IsNullOrWhiteSpace(Line))
					{
						Text.AppendLine("\t" + Line);
					}
				}
			}

			// Add remote-notifications as background mode
			if (bRemoteNotificationsSupported || bBackgroundFetch)
			{
				Text.AppendLine("\t<key>UIBackgroundModes</key>");
				Text.AppendLine("\t<array>");
                if (bRemoteNotificationsSupported)
                {
				    Text.AppendLine("\t\t<string>remote-notification</string>");
                }
                if (bBackgroundFetch)
                {
                    Text.AppendLine("\t\t<string>fetch</string>");
                }
				Text.AppendLine("\t</array>");
			}

			// write the iCloud container identifier, if present in the old file
			if (!string.IsNullOrEmpty(OldPListData))
			{
				int index = OldPListData.IndexOf("ICloudContainerIdentifier");
				if (index > 0)
				{
					index = OldPListData.IndexOf("<string>", index) + 8;
					int length = OldPListData.IndexOf("</string>", index) - index;
					string ICloudContainerIdentifier = OldPListData.Substring(index, length);
					Text.AppendLine("\t<key>ICloudContainerIdentifier</key>");
					Text.AppendLine(string.Format("\t<string>{0}</string>", ICloudContainerIdentifier));
				}
			}

			Text.AppendLine("</dict>");
			Text.AppendLine("</plist>");

			// Create the intermediate directory if needed
			if (!Directory.Exists(IntermediateDirectory))
			{
				Directory.CreateDirectory(IntermediateDirectory);
			}

			if (UPL != null)
			{
				// Allow UPL to modify the plist here
				XDocument XDoc;
				try
				{
					XDoc = XDocument.Parse(Text.ToString());
				}
				catch (Exception e)
				{
					throw new BuildException("plist is invalid {0}\n{1}", e, Text.ToString());
				}

				XDoc.DocumentType!.InternalSubset = "";
				UPL.ProcessPluginNode("None", "iosPListUpdates", "", ref XDoc);
				string result = XDoc.Declaration?.ToString() + "\n" + XDoc.ToString().Replace("<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\"[]>", "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">");
				File.WriteAllText(PListFile, result);

				Text = new StringBuilder(result);
			}
#endif
		}

		public static string Plist(string Command, string PlistFile)
		{
			Command = Command.Replace("\"", "\\\"");
			string Output = Utils.RunLocalProcessAndReturnStdOut("/usr/libexec/PlistBuddy", $"-c \"{Command}\" \"{PlistFile}\"");

//			Console.WriteLine($"{Command} ==> {Output}");

			return Output;
		}

		private string Plist(string Command)
		{
			return Plist(Command, PBXFilePathForPlatform(CurrentPlistPlatform).FullName);
		}

		private void PlistSetAdd(string Entry, string Value, string Type="string")
		{
			string AddOutput = Plist($"Add {Entry} {Type} {Value}");
			// error will be non-empty string
			if (AddOutput != "")
			{
				Plist($"Set {Entry} {Value}");
			}
		}

		private bool PlistSetUpdate(string Entry, string Value)
		{
			// see if the setting is already there
			string ExistingSetting = Plist($"Print {Entry}");

			// Print errors start with Print
			if (!ExistingSetting.StartsWith("Print:") && ExistingSetting != Value)
			{
				Plist($"Set {Entry} {Value}");

				return true;
			}

			return false;
		}

		private IEnumerable<string> PlistArray(string Entry)
		{
			return Plist($"Print {Entry}")
				.Replace("Array {", "")
				.Replace("}", "")
				.Trim()
				.ReplaceLineEndings()
				.Split(Environment.NewLine)
				.Select(x => x.Trim());
		}

		private List<string> PlistObjects()
		{
			List<string> Result = new();

			IEnumerable<string> Lines = Plist("print :objects")
				.ReplaceLineEndings()
				.Split(Environment.NewLine);

			Regex Regex = new Regex("^    (\\S*) = Dict {$");
			foreach (string Line in Lines)
			{
				Match Match = Regex.Match(Line);
				if (Match.Success)
				{
					Result.Add(Match.Groups[1].Value);
				}
			}
			return Result;
		}

		private string? PlistFixPath(string Entry, string RelativeToProject)
		{
			string ExistingPath = Plist($"Print {Entry}");
			// skip of errors, or it's an absolute path
			if (!ExistingPath.StartsWith("Print:") && !ExistingPath.StartsWith("/"))
			{
				// fixup the path to be relative to new project instead of old
				string FixedPath = Utils.CollapseRelativeDirectories(Path.Combine(RelativeToProject, ExistingPath));
				// and set it back
				Plist($"Set {Entry} {FixedPath}");

				return FixedPath;
			}

			return null;
		}


		bool MergeIntoTemplateProject(FileReference PBXProjFilePath, XcodeProject RootProject, FileReference TemplateProject)
		{
			// copy existing template project to final location
			if (FileReference.Exists(PBXProjFilePath))
			{
				FileReference.Delete(PBXProjFilePath);
			}
			DirectoryReference.CreateDirectory(PBXProjFilePath.Directory);
			FileReference.Copy(TemplateProject, PBXProjFilePath);

			// write the nodes we need to add (Build/Index targets)
			XcodeRunTarget RunTarget = XcodeProjectNode.GetNodesOfType<XcodeRunTarget>(RootProject).First();
			XcodeBuildTarget BuildTarget = XcodeProjectNode.GetNodesOfType<XcodeBuildTarget>(RunTarget).First();
			XcodeIndexTarget IndexTarget = XcodeProjectNode.GetNodesOfType<XcodeIndexTarget>(RootProject).First();
			XcodeDependency BuildDependency = XcodeProjectNode.GetNodesOfType<XcodeDependency>(RunTarget).First();

			// the runtarget and project need to write out so all of their xcconfigs get written as well,
			// so write everything to a temp string that is tossed, but all xcconfigs will be done at least
			StringBuilder Temp = new StringBuilder();
			XcodeProjectNode.WriteNodeAndReferences(Temp, RootProject!);

			StringBuilder Content = new StringBuilder();
			Content.WriteLine(0, "{");
			FileCollection.Write(Content);
			XcodeProjectNode.WriteNodeAndReferences(Content, BuildTarget);
			XcodeProjectNode.WriteNodeAndReferences(Content, IndexTarget);
			XcodeProjectNode.WriteNodeAndReferences(Content, BuildDependency);
			Content.WriteLine(0, "}");

			// write to disk
			FileReference ImportFile = FileReference.Combine(PBXProjFilePath.Directory, "import.plist");
			File.WriteAllText(ImportFile.FullName, Content.ToString());


			// cache some standard guids from the template project
			string ProjectGuid = Plist($"Print :rootObject");
			string TemplateMainGroupGuid = Plist($"Print :objects:{ProjectGuid}:mainGroup");

			// fixup paths that were relative to original project to be relative to merged project
//			List<string> ObjectGuids = PlistObjects();
			IEnumerable<string> MainGroupChildrenGuids = PlistArray($":objects:{TemplateMainGroupGuid}:children");

			string RelativeFromMergedToTemplate = TemplateProject.Directory.ParentDirectory!.MakeRelativeTo(PBXProjFilePath.Directory.ParentDirectory!);

			// look for groups with a 'path' element that is in the main group, so that it and everything will get redirected to new location
			string? FixedPath;
			foreach (string ChildGuid in MainGroupChildrenGuids)
			{
				string IsA = Plist($"Print :objects:{ChildGuid}:isa");
				// if a Group has a path
				if (IsA == "PBXGroup")
				{
					if ((FixedPath = PlistFixPath($":objects:{ChildGuid}:path", RelativeFromMergedToTemplate)) != null)
					{
						// if there wasn't a name before, it will now have a nasty path as the name, so add it now
						PlistSetAdd($":objects:{ChildGuid}:name", Path.GetFileName(FixedPath));
					}
				}
			}

			// and import it into the template
			Plist($"Merge \"{ImportFile.FullName}\" :objects");


			// get all the targets in the template that are application types
			IEnumerable<string> AppTargetGuids = PlistArray($":objects:{ProjectGuid}:targets")
				.Where(TargetGuid => (Plist($"Print :objects:{TargetGuid}:productType") == "com.apple.product-type.application"));

			// add a dependency on the build target from the app target(s)
			foreach (string AppTargetGuid in AppTargetGuids)
			{
				Plist($"Add :objects:{AppTargetGuid}:dependencies:0 string {BuildDependency.Guid}");
			}

			// the BuildDependency object was in the "container" of the generated project, not the merged one, so fix it up now
			Plist($"Set :objects:{BuildDependency.ProxyGuid}:containerPortal {ProjectGuid}");

			// now add all the non-run targets from the generated
			foreach (XcodeTarget Target in XcodeProjectNode.GetNodesOfType<XcodeTarget>(RootProject!).Where(x => x.GetType() != typeof(XcodeRunTarget)))
			{
				Plist($"Add :objects:{ProjectGuid}:targets:0 string {Target.Guid}");
			}


			// hook up Xcconfig files to the project and the project configs
			// @todo how to manage with conflicts already present...
			//PlistSetAdd($":objects:{ProjectGuid}:baseConfigurationReference", RootProject.Xcconfig!.Guid, "string");

			// re-get the list of targets now that we merged in the other file
			IEnumerable<string> AllTargetGuids = PlistArray($":objects:{ProjectGuid}:targets");

			List<string> NodesToFix = new() { ProjectGuid };
			NodesToFix.AddRange(AllTargetGuids);

			bool bIsProject = true;
			foreach (string NodeGuid in NodesToFix)
			{
				bool bIsAppTarget = AppTargetGuids.Contains(NodeGuid);

				// get the config list, and from there we can get the configs
				string ProjectBuildConfigListGuid = Plist($"Print :objects:{NodeGuid}:buildConfigurationList");

				IEnumerable<string> ConfigGuids = PlistArray($":objects:{ProjectBuildConfigListGuid}:buildConfigurations");
				foreach (string ConfigGuid in ConfigGuids)
				{
					// find the matching unreal generated project build config to hook up to
					// for now we assume Release is Development [Editor], but we should make sure the template project has good configs
					// we have to rename the template config from Release because it won't find the matching config in the build target
					string ConfigName = Plist($"Print :objects:{ConfigGuid}:name");
					if (ConfigName == "Release")
					{
						ConfigName = UnrealData.bHasEditorConfiguration ? "Development Editor" : "Development";
						Plist($"Set :objects:{ConfigGuid}:name \"{ConfigName}\"");
					}

					// if there's a plist path, then it will need to be fixed up
					PlistFixPath($":objects:{ConfigGuid}:buildSettings:INFOPLIST_FILE", RelativeFromMergedToTemplate);

					if (bIsProject)
					{
						//Console.WriteLine("Looking for " + ConfigName);
						XcodeBuildConfig Config = RootProject!.ProjectBuildConfigs.BuildConfigs.First(x => x.Info.DisplayName == ConfigName);
						PlistSetAdd($":objects:{ConfigGuid}:baseConfigurationReference", Config.Xcconfig!.Guid, "string");
					}

					// the Build target used some ini settings to compile, and Run target must match, so we override a few settings, at
					// whatever level they were already specified at (Projet and/or Target)
					PlistSetUpdate($":objects:{ConfigGuid}:buildSettings:MACOSX_DEPLOYMENT_TARGET", MacToolChain.Settings.MacOSVersion);
					if (UnrealData.IOSProjectSettings != null)
					{
						PlistSetUpdate($":objects:{ConfigGuid}:buildSettings:IPHONEOS_DEPLOYMENT_TARGET", UnrealData.IOSProjectSettings.RuntimeVersion);
					}
					if (UnrealData.TVOSProjectSettings != null)
					{
						PlistSetUpdate($":objects:{ConfigGuid}:buildSettings:TVOS_DEPLOYMENT_TARGET", UnrealData.TVOSProjectSettings.RuntimeVersion);
					}
				}

				bIsProject = false;
			}

			// now we need to merge the main groups together
			string GeneratedMainGroupGuid = FileCollection.MainGroupGuid;
			int Index = 0;
			while (true)
			{
				// we copy to a high index to put the copied entries at the end in the same order
				string Output = Plist($"Copy :objects:{GeneratedMainGroupGuid}:children:{Index} :objects:{TemplateMainGroupGuid}:children:100000000");

				// loop until error
				if (Output != "")
				{
					break;
				}
				Index++;
			}
			// and remove the one we copied from
			Plist($"Delete :objects:{GeneratedMainGroupGuid}");

			return true;
		}

#region Schemes

		private FileReference GetUserSchemeManagementFilePath(UnrealTargetPlatform? Platform)
		{
			return FileReference.Combine(ProjectDirPathForPlatform(Platform), $"xcuserdata/{Environment.UserName}.xcuserdatad/xcschemes/xcschememanagement.plist");
		}

		private DirectoryReference GetProjectSchemeDirectory(UnrealTargetPlatform? Platform)
		{
			return DirectoryReference.Combine(ProjectDirPathForPlatform(Platform), "xcshareddata/xcschemes");
		}

		private FileReference GetProjectSchemeFilePathForTarget(UnrealTargetPlatform? Platform, string TargetName)
		{
			return FileReference.Combine(GetProjectSchemeDirectory(Platform), TargetName + ".xcscheme");
		}

		private void WriteSchemeFile(UnrealTargetPlatform? Platform, string ProjectName, List<XcodeRunTarget> RunTargets, string? BuildTargetGuid, string? IndexTargetGuid, bool bHasEditorConfiguration, string GameProjectPath)
		{
			StringBuilder Content = new StringBuilder();

			foreach (XcodeRunTarget RunTarget in RunTargets)
			{
				string TargetName = RunTarget.Name;
				string TargetGuid = RunTarget.Guid;
				bHasEditorConfiguration = RunTarget.BuildConfigList!.BuildConfigs.Any(x => x.Info.ProjectTarget!.TargetRules!.Type == TargetType.Editor);

				FileReference SchemeFilePath = GetProjectSchemeFilePathForTarget(Platform, TargetName);

				DirectoryReference.CreateDirectory(SchemeFilePath.Directory);

				string? OldCommandLineArguments = null;
				if (FileReference.Exists(SchemeFilePath))
				{
					string OldContents = File.ReadAllText(SchemeFilePath.FullName);
					int OldCommandLineArgumentsStart = OldContents.IndexOf("<CommandLineArguments>") + "<CommandLineArguments>".Length;
					int OldCommandLineArgumentsEnd = OldContents.IndexOf("</CommandLineArguments>");
					if (OldCommandLineArgumentsStart != -1 && OldCommandLineArgumentsEnd != -1)
					{
						OldCommandLineArguments = OldContents.Substring(OldCommandLineArgumentsStart, OldCommandLineArgumentsEnd - OldCommandLineArgumentsStart);
					}
				}

				string DefaultConfiguration = (bHasEditorConfiguration && !XcodeProjectFileGenerator.bGenerateRunOnlyProject) ? "Development Editor" : "Development";

				Content.WriteLine("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
				Content.WriteLine("<Scheme");
				Content.WriteLine("   LastUpgradeVersion = \"2000\"");
				Content.WriteLine("   version = \"1.3\">");
				Content.WriteLine("   <BuildAction");
				Content.WriteLine("      parallelizeBuildables = \"YES\"");
				Content.WriteLine("      buildImplicitDependencies = \"YES\">");
				Content.WriteLine("      <BuildActionEntries>");
				Content.WriteLine("         <BuildActionEntry");
				Content.WriteLine("            buildForTesting = \"YES\"");
				Content.WriteLine("            buildForRunning = \"YES\"");
				Content.WriteLine("            buildForProfiling = \"YES\"");
				Content.WriteLine("            buildForArchiving = \"YES\"");
				Content.WriteLine("            buildForAnalyzing = \"YES\">");
				Content.WriteLine("            <BuildableReference");
				Content.WriteLine("               BuildableIdentifier = \"primary\"");
				Content.WriteLine("               BlueprintIdentifier = \"" + TargetGuid + "\"");
				Content.WriteLine("               ReferencedContainer = \"container:" + ProjectName + ".xcodeproj\">");
				Content.WriteLine("            </BuildableReference>");
				Content.WriteLine("         </BuildActionEntry>");
				Content.WriteLine("      </BuildActionEntries>");
				Content.WriteLine("   </BuildAction>");
				Content.WriteLine("   <TestAction");
				Content.WriteLine("      buildConfiguration = \"" + DefaultConfiguration + "\"");
				Content.WriteLine("      selectedDebuggerIdentifier = \"Xcode.DebuggerFoundation.Debugger.LLDB\"");
				Content.WriteLine("      selectedLauncherIdentifier = \"Xcode.DebuggerFoundation.Launcher.LLDB\"");
				Content.WriteLine("      shouldUseLaunchSchemeArgsEnv = \"YES\">");
				Content.WriteLine("      <Testables>");
				Content.WriteLine("      </Testables>");
				Content.WriteLine("      <MacroExpansion>");
				Content.WriteLine("            <BuildableReference");
				Content.WriteLine("               BuildableIdentifier = \"primary\"");
				Content.WriteLine("               BlueprintIdentifier = \"" + TargetGuid + "\"");
				Content.WriteLine("               ReferencedContainer = \"container:" + ProjectName + ".xcodeproj\">");
				Content.WriteLine("            </BuildableReference>");
				Content.WriteLine("      </MacroExpansion>");
				Content.WriteLine("      <AdditionalOptions>");
				Content.WriteLine("      </AdditionalOptions>");
				Content.WriteLine("   </TestAction>");
				Content.WriteLine("   <LaunchAction");
				Content.WriteLine("      buildConfiguration = \"" + DefaultConfiguration + "\"");
				Content.WriteLine("      selectedDebuggerIdentifier = \"Xcode.DebuggerFoundation.Debugger.LLDB\"");
				Content.WriteLine("      selectedLauncherIdentifier = \"Xcode.DebuggerFoundation.Launcher.LLDB\"");
				Content.WriteLine("      launchStyle = \"0\"");
				Content.WriteLine("      useCustomWorkingDirectory = \"NO\"");
				Content.WriteLine("      ignoresPersistentStateOnLaunch = \"NO\"");
				Content.WriteLine("      debugDocumentVersioning = \"NO\"");
				Content.WriteLine("      debugServiceExtension = \"internal\"");
				Content.WriteLine("      allowLocationSimulation = \"YES\">");
				Content.WriteLine("      <BuildableProductRunnable");
				Content.WriteLine("         runnableDebuggingMode = \"0\">");
				Content.WriteLine("            <BuildableReference");
				Content.WriteLine("               BuildableIdentifier = \"primary\"");
				Content.WriteLine("               BlueprintIdentifier = \"" + TargetGuid + "\"");
				Content.WriteLine("               ReferencedContainer = \"container:" + ProjectName + ".xcodeproj\">");
				Content.WriteLine("            </BuildableReference>");
				Content.WriteLine("      </BuildableProductRunnable>");
				if (string.IsNullOrEmpty(OldCommandLineArguments))
				{
					if (bHasEditorConfiguration && TargetName != "UE5")
					{
						Content.WriteLine("      <CommandLineArguments>");
						if (IsForeignProject)
						{
							Content.WriteLine("         <CommandLineArgument");
							Content.WriteLine("            argument = \"&quot;" + GameProjectPath + "&quot;\"");
							Content.WriteLine("            isEnabled = \"YES\">");
							Content.WriteLine("         </CommandLineArgument>");
						}
						else
						{
							Content.WriteLine("         <CommandLineArgument");
							Content.WriteLine("            argument = \"" + TargetName + "\"");
							Content.WriteLine("            isEnabled = \"YES\">");
							Content.WriteLine("         </CommandLineArgument>");
						}
						// Always add a configuration argument
						Content.WriteLine("         <CommandLineArgument");
						Content.WriteLine("            argument = \"-RunConfig=$(Configuration)\"");
						Content.WriteLine("            isEnabled = \"YES\">");
						Content.WriteLine("         </CommandLineArgument>");
						Content.WriteLine("      </CommandLineArguments>");
					}
				}
				else
				{
					Content.WriteLine("      <CommandLineArguments>" + OldCommandLineArguments + "</CommandLineArguments>");
				}
				Content.WriteLine("      <AdditionalOptions>");
				Content.WriteLine("      </AdditionalOptions>");
				Content.WriteLine("   </LaunchAction>");
				Content.WriteLine("   <ProfileAction");
				Content.WriteLine("      buildConfiguration = \"" + DefaultConfiguration + "\"");
				Content.WriteLine("      shouldUseLaunchSchemeArgsEnv = \"YES\"");
				Content.WriteLine("      savedToolIdentifier = \"\"");
				Content.WriteLine("      useCustomWorkingDirectory = \"NO\"");
				Content.WriteLine("      debugDocumentVersioning = \"NO\">");
				Content.WriteLine("      <BuildableProductRunnable");
				Content.WriteLine("         runnableDebuggingMode = \"0\">");
				Content.WriteLine("            <BuildableReference");
				Content.WriteLine("               BuildableIdentifier = \"primary\"");
				Content.WriteLine("               BlueprintIdentifier = \"" + TargetGuid + "\"");
				Content.WriteLine("               BuildableName = \"" + ProjectName + ".app\"");
				Content.WriteLine("               BlueprintName = \"" + TargetName + "\"");
				Content.WriteLine("               ReferencedContainer = \"container:" + ProjectName + ".xcodeproj\">");
				Content.WriteLine("            </BuildableReference>");
				Content.WriteLine("      </BuildableProductRunnable>");
				Content.WriteLine("   </ProfileAction>");
				Content.WriteLine("   <AnalyzeAction");
				Content.WriteLine("      buildConfiguration = \"" + DefaultConfiguration + "\">");
				Content.WriteLine("   </AnalyzeAction>");
				Content.WriteLine("   <ArchiveAction");
				Content.WriteLine("      buildConfiguration = \"" + DefaultConfiguration + "\"");
				Content.WriteLine("      revealArchiveInOrganizer = \"YES\">");
				Content.WriteLine("   </ArchiveAction>");
				Content.WriteLine("</Scheme>");

				File.WriteAllText(SchemeFilePath.FullName, Content.ToString(), new UTF8Encoding());
				Content.Clear();
			}

			Content.WriteLine("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
			Content.WriteLine("<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">");
			Content.WriteLine("<plist version=\"1.0\">");
			Content.WriteLine("<dict>");
			Content.WriteLine("\t<key>SchemeUserState</key>");
			Content.WriteLine("\t<dict>");
			foreach (XcodeRunTarget RunTarget in RunTargets)
			{
				Content.WriteLine("\t\t<key>" + RunTarget.Name + ".xcscheme_^#shared#^_</key>");
				Content.WriteLine("\t\t<dict>");
				Content.WriteLine("\t\t\t<key>orderHint</key>");
				Content.WriteLine("\t\t\t<integer>1</integer>");
				Content.WriteLine("\t\t</dict>");
			}
			Content.WriteLine("\t</dict>");
			Content.WriteLine("\t<key>SuppressBuildableAutocreation</key>");
			Content.WriteLine("\t<dict>");
			foreach (XcodeRunTarget RunTarget in RunTargets)
			{
				Content.WriteLine("\t\t<key>" + RunTarget.Guid + "</key>");
				Content.WriteLine("\t\t<dict>");
				Content.WriteLine("\t\t\t<key>primary</key>");
				Content.WriteLine("\t\t\t<true/>");
				Content.WriteLine("\t\t</dict>");
			}
			if (BuildTargetGuid != null)
			{
				Content.WriteLine("\t\t<key>" + BuildTargetGuid + "</key>");
				Content.WriteLine("\t\t<dict>");
				Content.WriteLine("\t\t\t<key>primary</key>");
				Content.WriteLine("\t\t\t<true/>");
				Content.WriteLine("\t\t</dict>");
			}
			if (IndexTargetGuid != null)
			{
				Content.WriteLine("\t\t<key>" + IndexTargetGuid + "</key>");
				Content.WriteLine("\t\t<dict>");
				Content.WriteLine("\t\t\t<key>primary</key>");
				Content.WriteLine("\t\t\t<true/>");
				Content.WriteLine("\t\t</dict>");
			}
			Content.WriteLine("\t</dict>");
			Content.WriteLine("</dict>");
			Content.WriteLine("</plist>");

			FileReference ManagementFile = GetUserSchemeManagementFilePath(Platform);
			if (!DirectoryReference.Exists(ManagementFile.Directory))
			{
				DirectoryReference.CreateDirectory(ManagementFile.Directory);
			}

			File.WriteAllText(ManagementFile.FullName, Content.ToString(), new UTF8Encoding());
		}

#endregion


#region Utilities

		public override string ToString()
		{
			throw new NotImplementedException();
//			return ProjectFilePath.GetFileNameWithoutExtension();
		}

#endregion

	}
}
