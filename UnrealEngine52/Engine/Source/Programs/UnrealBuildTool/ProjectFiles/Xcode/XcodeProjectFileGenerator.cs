// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using EpicGames.Core;
using System.Linq;
using Microsoft.Extensions.Logging;

namespace UnrealBuildTool
{
	enum XcodePerPlatformMode
	{
		OneWorkspacePerPlatform,
		OneTargetPerPlatform,
	}

	/// <summary>
	/// Xcode project file generator implementation
	/// </summary>
	class XcodeProjectFileGenerator : ProjectFileGenerator
	{
		public DirectoryReference? XCWorkspace;

		// always seed the random number the same, so multiple runs of the generator will generate the same project
		static Random Rand = new Random(0);

		// how to handle multiple platforms
		public static XcodePerPlatformMode PerPlatformMode = XcodePerPlatformMode.OneWorkspacePerPlatform;

		/// <summary>
		/// Mark for distribution builds
		/// </summary>
		bool bForDistribution = false;

		/// <summary>
		/// Override BundleID
		/// </summary>
		string BundleIdentifier = "";

		/// <summary>
		/// Override AppName
		/// </summary>
		string AppName = "";

		/// <summary>
		/// Store the single game project (when using -game -project=...) to a place that XcodeProjectLegacy can easily retrieve it
		/// </summary>
		public static FileReference? SingleGameProject = null;
		
		public XcodeProjectFileGenerator(FileReference? InOnlyGameProject, CommandLineArguments CommandLine)
			: base(InOnlyGameProject)
		{
			SingleGameProject = InOnlyGameProject;
			
			if (CommandLine.HasOption("-distribution"))
			{
				bForDistribution = true;
			}
			if (CommandLine.HasValue("-bundleID="))
			{
				BundleIdentifier = CommandLine.GetString("-bundleID=");
			}

			if (CommandLine.HasValue("-appname="))
			{
				AppName = CommandLine.GetString("-appname=");
			}
		}

		/// <summary>
		/// Make a random Guid string usable by Xcode (24 characters exactly)
		/// </summary>
		public static string MakeXcodeGuid()
		{
			string Guid = "";

			byte[] Randoms = new byte[12];
			Rand.NextBytes(Randoms);
			for (int Index = 0; Index < 12; Index++)
			{
				Guid += Randoms[Index].ToString("X2");
			}

			return Guid;
		}

		/// File extension for project files we'll be generating (e.g. ".vcxproj")
		override public string ProjectFileExtension
		{
			get
			{
				return ".xcodeproj";
			}
		}

		/// <summary>
		/// </summary>
		public override void CleanProjectFiles(DirectoryReference InPrimaryProjectDirectory, string InPrimaryProjectName, DirectoryReference InIntermediateProjectFilesPath, ILogger Logger)
		{
			foreach (DirectoryReference ProjectFile in DirectoryReference.EnumerateDirectories(InPrimaryProjectDirectory, $"{InPrimaryProjectName}*.xcworkspace"))
			{
				DirectoryReference.Delete(ProjectFile, true);
			}

			// Delete the project files folder
			if (DirectoryReference.Exists(InIntermediateProjectFilesPath))
			{
				try
				{
					DirectoryReference.Delete(InIntermediateProjectFilesPath, true);
				}
				catch (Exception Ex)
				{
					Logger.LogInformation("Error while trying to clean project files path {InIntermediateProjectFilesPath}. Ignored.", InIntermediateProjectFilesPath);
					Logger.LogInformation("\t{Ex}", Ex.Message);
				}
			}
		}

		/// <summary>
		/// Allocates a generator-specific project file object
		/// </summary>
		/// <param name="InitFilePath">Path to the project file</param>
		/// <param name="BaseDir">The base directory for files within this project</param>
		/// <returns>The newly allocated project file object</returns>
		protected override ProjectFile AllocateProjectFile(FileReference InitFilePath, DirectoryReference BaseDir)
		{
			// this may internally (later) make a Legacy project object if the unreal project wants old behavior
			// unfortunately, we can't read the project configs now because we don't have enough information to 
			// find the .uproject file for that would make this project (we could change the high level to pass it 
			// down but it would touch all project generators - not worth it if we end up removing the legacy)
			return new XcodeProjectXcconfig.XcodeProjectFile(InitFilePath, BaseDir, bForDistribution, BundleIdentifier, AppName);
		}

		private bool WriteWorkspaceSettingsFile(string Path, ILogger Logger)
		{
			StringBuilder WorkspaceSettingsContent = new StringBuilder();
			WorkspaceSettingsContent.Append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>" + ProjectFileGenerator.NewLine);
			WorkspaceSettingsContent.Append("<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">" + ProjectFileGenerator.NewLine);
			WorkspaceSettingsContent.Append("<plist version=\"1.0\">" + ProjectFileGenerator.NewLine);
			WorkspaceSettingsContent.Append("<dict>" + ProjectFileGenerator.NewLine);
			// @todo when we move to xcode 14, we remove these next 4 keys
			WorkspaceSettingsContent.Append("\t<key>BuildSystemType</key>" + ProjectFileGenerator.NewLine);
			WorkspaceSettingsContent.Append("\t<string>Original</string>" + ProjectFileGenerator.NewLine);
			WorkspaceSettingsContent.Append("\t<key>BuildLocationStyle</key>" + ProjectFileGenerator.NewLine);
			WorkspaceSettingsContent.Append("\t<string>UseTargetSettings</string>" + ProjectFileGenerator.NewLine);
			WorkspaceSettingsContent.Append("\t<key>CustomBuildLocationType</key>" + ProjectFileGenerator.NewLine);
			WorkspaceSettingsContent.Append("\t<string>RelativeToDerivedData</string>" + ProjectFileGenerator.NewLine);
			WorkspaceSettingsContent.Append("\t<key>DerivedDataLocationStyle</key>" + ProjectFileGenerator.NewLine);
			WorkspaceSettingsContent.Append("\t<string>Default</string>" + ProjectFileGenerator.NewLine);
			WorkspaceSettingsContent.Append("\t<key>IssueFilterStyle</key>" + ProjectFileGenerator.NewLine);
			WorkspaceSettingsContent.Append("\t<string>ShowAll</string>" + ProjectFileGenerator.NewLine);
			WorkspaceSettingsContent.Append("\t<key>LiveSourceIssuesEnabled</key>" + ProjectFileGenerator.NewLine);
			WorkspaceSettingsContent.Append("\t<true/>" + ProjectFileGenerator.NewLine);
			WorkspaceSettingsContent.Append("\t<key>SnapshotAutomaticallyBeforeSignificantChanges</key>" + ProjectFileGenerator.NewLine);
			WorkspaceSettingsContent.Append("\t<true/>" + ProjectFileGenerator.NewLine);
			WorkspaceSettingsContent.Append("\t<key>SnapshotLocationStyle</key>" + ProjectFileGenerator.NewLine);
			WorkspaceSettingsContent.Append("\t<string>Default</string>" + ProjectFileGenerator.NewLine);
			WorkspaceSettingsContent.Append("</dict>" + ProjectFileGenerator.NewLine);
			WorkspaceSettingsContent.Append("</plist>" + ProjectFileGenerator.NewLine);
			return WriteFileIfChanged(Path, WorkspaceSettingsContent.ToString(), Logger, new UTF8Encoding());
		}

		private bool WriteWorkspaceSharedSettingsFile(string Path, ILogger Logger)
		{
			StringBuilder WorkspaceSettingsContent = new StringBuilder();
			WorkspaceSettingsContent.Append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>" + ProjectFileGenerator.NewLine);
			WorkspaceSettingsContent.Append("<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">" + ProjectFileGenerator.NewLine);
			WorkspaceSettingsContent.Append("<plist version=\"1.0\">" + ProjectFileGenerator.NewLine);
			WorkspaceSettingsContent.Append("<dict>" + ProjectFileGenerator.NewLine);
			// @todo when we move to xcode 14, we remove these next 2 keys
			WorkspaceSettingsContent.Append("\t<key>DisableBuildSystemDeprecationWarning</key>" + ProjectFileGenerator.NewLine);
			WorkspaceSettingsContent.Append("\t<true/>" + ProjectFileGenerator.NewLine);
			WorkspaceSettingsContent.Append("\t<key>DisableBuildSystemDeprecationDiagnostic</key>" + ProjectFileGenerator.NewLine);
			WorkspaceSettingsContent.Append("\t<true/>" + ProjectFileGenerator.NewLine);
			WorkspaceSettingsContent.Append("\t<key>IDEWorkspaceSharedSettings_AutocreateContextsIfNeeded</key>" + ProjectFileGenerator.NewLine);
			WorkspaceSettingsContent.Append("\t<false/>" + ProjectFileGenerator.NewLine);
			WorkspaceSettingsContent.Append("</dict>" + ProjectFileGenerator.NewLine);
			WorkspaceSettingsContent.Append("</plist>" + ProjectFileGenerator.NewLine);
			return WriteFileIfChanged(Path, WorkspaceSettingsContent.ToString(), Logger, new UTF8Encoding());
		}

		private string PrimaryProjectNameForPlatform(UnrealTargetPlatform? Platform)
		{
			return Platform == null ? PrimaryProjectName : $"{PrimaryProjectName} ({Platform})";
		}

		private bool WriteXcodeWorkspace(ILogger Logger)
		{
			bool bSuccess = true;

			// loop opver all projects to see if at least one is modern (if not, we don't bother splitting up by platform)
			bool bHasModernProjects = false;
			Action<List<PrimaryProjectFolder>>? FindModern = null;
			FindModern = (FolderList) =>
			{
				foreach (PrimaryProjectFolder CurFolder in FolderList)
				{
					var Modern = CurFolder.ChildProjects.FirstOrDefault(P =>
						P.GetType() == typeof(XcodeProjectXcconfig.XcodeProjectFile) &&
						!((XcodeProjectXcconfig.XcodeProjectFile)P).bHasLegacyProject);
					if (Modern != null)
					{
						//Logger.LogWarning($"Project {Modern.ProjectFilePath} is modern");
						bHasModernProjects = true;
					}
					if (!bHasModernProjects)
					{
						FindModern!(CurFolder.SubFolders);
					}
				}
			};
			FindModern(RootFolder.SubFolders);

			// if we want one workspace with multiple platforms, and we have at least one modern project, then process each platform individually
			// otherwise use null as Platform which means to merge all platforms
			List<UnrealTargetPlatform?> PlatformsToProcess = bHasModernProjects ? WorkspacePlatforms : NullPlatformList; 
			foreach (UnrealTargetPlatform? Platform in PlatformsToProcess)
			{
				StringBuilder WorkspaceDataContent = new();
				WorkspaceDataContent.Append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>" + ProjectFileGenerator.NewLine);
				WorkspaceDataContent.Append("<Workspace" + ProjectFileGenerator.NewLine);
				WorkspaceDataContent.Append("   version = \"1.0\">" + ProjectFileGenerator.NewLine);

				List<XcodeProjectXcconfig.XcodeProjectFile> BuildableProjects = new();

				System.Action<List<PrimaryProjectFolder> /* Folders */, string /* Ident */ >? AddProjectsFunction = null;
				AddProjectsFunction = (FolderList, Ident) =>
					{
						foreach (PrimaryProjectFolder CurFolder in FolderList)
						{
							WorkspaceDataContent.Append(Ident + "   <Group" + ProjectFileGenerator.NewLine);
							WorkspaceDataContent.Append(Ident + "      location = \"container:\"      name = \"" + CurFolder.FolderName + "\">" + ProjectFileGenerator.NewLine);

							AddProjectsFunction!(CurFolder.SubFolders, Ident + "   ");

							// Filter out anything that isn't an XC project, and that shouldn't be in the workspace
							IEnumerable<XcodeProjectXcconfig.XcodeProjectFile> SupportedProjects =
									CurFolder.ChildProjects.Where(P => P.GetType() == typeof(XcodeProjectXcconfig.XcodeProjectFile))
										.Select(P => (XcodeProjectXcconfig.XcodeProjectFile)P)
										.Where(P => XcodeProjectXcconfig.UnrealData.ShouldIncludeProjectInWorkspace(P, Logger))
										// @todo - still need to handle legacy project getting split up?
										.Where(P => P.RootProjects.Count == 0 || P.RootProjects.ContainsValue(Platform))
										.OrderBy(P => P.ProjectFilePath.GetFileName());

							foreach (XcodeProjectXcconfig.XcodeProjectFile XcodeProject in SupportedProjects)
							{
								// we have to re-check for each project - if it's a legacy project, even if we wanted it split, it won't be, so always point to 
								// the shared legacy project
								FileReference PathToProject = XcodeProject.ProjectFilePath;
								if (!XcodeProject.bHasLegacyProject && PerPlatformMode == XcodePerPlatformMode.OneWorkspacePerPlatform)
								{
									PathToProject = XcodeProject.ProjectFilePathForPlatform(Platform);
								}
								WorkspaceDataContent.Append(Ident + "      <FileRef" + ProjectFileGenerator.NewLine);
								WorkspaceDataContent.Append(Ident + "         location = \"group:" + PathToProject.MakeRelativeTo(ProjectFileGenerator.PrimaryProjectPath) + "\">" + ProjectFileGenerator.NewLine);
								WorkspaceDataContent.Append(Ident + "      </FileRef>" + ProjectFileGenerator.NewLine);
							}

							BuildableProjects.AddRange(SupportedProjects);

							WorkspaceDataContent.Append(Ident + "   </Group>" + ProjectFileGenerator.NewLine);
						}
					};
				AddProjectsFunction(RootFolder.SubFolders, "");

				WorkspaceDataContent.Append("</Workspace>" + ProjectFileGenerator.NewLine);

				// Also, update project's schemes index so that the schemes are in a sensible order
				// (Game, Editor, Client, Server, Programs)
				int SchemeIndex = 0;
				BuildableProjects.Sort((ProjA, ProjB) =>
				{

					ProjectTarget TargetA = ProjA.ProjectTargets.OfType<ProjectTarget>().OrderBy(T => T.TargetRules!.Type).First();
					ProjectTarget TargetB = ProjB.ProjectTargets.OfType<ProjectTarget>().OrderBy(T => T.TargetRules!.Type).First();

					TargetType TypeA = TargetA.TargetRules!.Type;
					TargetType TypeB = TargetB.TargetRules!.Type;

					if (TypeA != TypeB)
					{
						return TypeA.CompareTo(TypeB);
					}

					return TargetA.Name.CompareTo(TargetB.Name);
				});

				foreach (XcodeProjectXcconfig.XcodeProjectFile XcodeProject in BuildableProjects)
				{
					FileReference SchemeManagementFile = XcodeProject.ProjectFilePathForPlatform(Platform) + "/xcuserdata/" + Environment.UserName + ".xcuserdatad/xcschemes/xcschememanagement.plist";
					if (FileReference.Exists(SchemeManagementFile))
					{
						string SchemeManagementContent = FileReference.ReadAllText(SchemeManagementFile);
						SchemeManagementContent = SchemeManagementContent.Replace("<key>orderHint</key>\n\t\t\t<integer>1</integer>", "<key>orderHint</key>\n\t\t\t<integer>" + SchemeIndex.ToString() + "</integer>");
						FileReference.WriteAllText(SchemeManagementFile, SchemeManagementContent);
						SchemeIndex++;
					}
				}

				string ProjectName = PrimaryProjectNameForPlatform(Platform);
				string WorkspaceDataFilePath = PrimaryProjectPath + "/" + ProjectName + ".xcworkspace/contents.xcworkspacedata";
				Logger.LogInformation($"Writing xcode workspace {Path.GetDirectoryName(WorkspaceDataFilePath)}");
				bSuccess = WriteFileIfChanged(WorkspaceDataFilePath, WorkspaceDataContent.ToString(), Logger, new UTF8Encoding());
				if (bSuccess)
				{
					string WorkspaceSettingsFilePath = PrimaryProjectPath + "/" + ProjectName + ".xcworkspace/xcuserdata/" + Environment.UserName + ".xcuserdatad/WorkspaceSettings.xcsettings";
					bSuccess = WriteWorkspaceSettingsFile(WorkspaceSettingsFilePath, Logger);
					string WorkspaceSharedSettingsFilePath = PrimaryProjectPath + "/" + ProjectName + ".xcworkspace/xcshareddata/WorkspaceSettings.xcsettings";
					bSuccess = WriteWorkspaceSharedSettingsFile(WorkspaceSharedSettingsFilePath, Logger);

					// cache the location of the workspace, for users of this to know where the final workspace is
					XCWorkspace = new FileReference(WorkspaceDataFilePath).Directory;
				}
			}

			return bSuccess;
		}

		protected override bool WritePrimaryProjectFile(ProjectFile? UBTProject, PlatformProjectGeneratorCollection PlatformProjectGenerators, ILogger Logger)
		{
			return WriteXcodeWorkspace(Logger);
		}

		/// <summary>
		/// A static copy of ProjectPlatforms from the base class
		/// </summary>
		static public List<UnrealTargetPlatform> XcodePlatforms = new();

		static public List<UnrealTargetPlatform?> WorkspacePlatforms = new ();
		static public List<UnrealTargetPlatform?> RunTargetPlatforms = new();
		static public List<UnrealTargetPlatform?> NullPlatformList = new() { null };


		/// <summary>
		/// Should we generate only a run project (no build/index targets)
		/// </summary>
		static public bool bGenerateRunOnlyProject = false;

		/// <inheritdoc/>
		protected override void ConfigureProjectFileGeneration(string[] Arguments, ref bool IncludeAllPlatforms, ILogger Logger)
		{
			// Call parent implementation first
			base.ConfigureProjectFileGeneration(Arguments, ref IncludeAllPlatforms, Logger);

			if (ProjectPlatforms.Count > 0)
			{
				XcodePlatforms.AddRange(ProjectPlatforms);
			}
			else
			{
				// add platforms that have synced platform support
				if (InstalledPlatformInfo.IsValidPlatform(UnrealTargetPlatform.Mac, EProjectType.Code))
				{
					XcodePlatforms.Add(UnrealTargetPlatform.Mac);
				}
				if (InstalledPlatformInfo.IsValidPlatform(UnrealTargetPlatform.IOS, EProjectType.Code))
				{
					XcodePlatforms.Add(UnrealTargetPlatform.IOS);
				}
				if (InstalledPlatformInfo.IsValidPlatform(UnrealTargetPlatform.TVOS, EProjectType.Code))
				{
					XcodePlatforms.Add(UnrealTargetPlatform.TVOS);
				}
			}

			if (PerPlatformMode == XcodePerPlatformMode.OneWorkspacePerPlatform)
			{
				WorkspacePlatforms = XcodePlatforms.Select<UnrealTargetPlatform, UnrealTargetPlatform?>(x => x).ToList();
			}
			else
			{
				WorkspacePlatforms = NullPlatformList;
			}

			foreach (string CurArgument in Arguments)
			{
				if (CurArgument.Contains("-iOSDeployOnly", StringComparison.InvariantCultureIgnoreCase) ||
					CurArgument.Contains("-tvOSDeployOnly", StringComparison.InvariantCultureIgnoreCase) ||
					CurArgument.Contains("-DeployOnly", StringComparison.InvariantCultureIgnoreCase))
				{
					bGenerateRunOnlyProject = true;
					break;
				}
			}

			if (bGenerateRunOnlyProject)
			{
				bIncludeEnginePrograms = false;
				//bIncludeEngineSource = false;
				bIncludeTemplateFiles = false;
				bIncludeConfigFiles = false;
				bIncludeDocumentation = false;
				bIncludeShaderSource = false;

				// generate just the engine project
				if (OnlyGameProject == null)
				{
					bIncludeEngineSource = true;
				}
				// generate just the game project
				else
				{
					bGeneratingGameProjectFiles = true;
				}
			}
		}
	}
}
