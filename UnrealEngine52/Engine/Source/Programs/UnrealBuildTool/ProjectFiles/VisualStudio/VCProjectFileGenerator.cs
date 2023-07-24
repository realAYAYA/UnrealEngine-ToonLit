// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Xml.Linq;
using EpicGames.Core;
using UnrealBuildBase;
using Microsoft.Extensions.Logging;

namespace UnrealBuildTool
{
	enum VCProjectFileFormat
	{
		Default,          // Default to the best installed version, but allow SDKs to override
		VisualStudio2019,
		VisualStudio2022,
	}

	class VCProjectFileSettings
	{
		/// <summary>
		/// The version of Visual Studio to generate project files for.
		/// </summary>
		[XmlConfigFile(Category = "VCProjectFileGenerator", Name = "Version")]
		public VCProjectFileFormat ProjectFileFormat = VCProjectFileFormat.Default;

		/// <summary>
		/// Puts the most common include paths in the IncludePath property in the MSBuild project. This significantly reduces Visual Studio
		/// memory usage (measured 1.1GB -> 500mb), but seems to be causing issues with Visual Assist. Value here specifies maximum length
		/// of the include path list in KB.
		/// </summary>
		[XmlConfigFile(Category = "VCProjectFileGenerator")]
		public int MaxSharedIncludePaths = 24 * 1024;

		/// <summary>
		/// Semi-colon separated list of paths that should not be added to the projects include paths. Useful for omitting third-party headers
		/// (e.g ThirdParty/WebRTC) from intellisense suggestions and reducing memory footprints.
		/// </summary>
		[XmlConfigFile(Category = "VCProjectFileGenerator")]
		public string ExcludedIncludePaths = "";

		/// <summary>
		/// Semi-colon separated list of paths that should not be added to the projects. Useful for omitting third-party files
		/// (e.g ThirdParty/WebRTC) from intellisense suggestions and reducing memory footprints.
		/// </summary>
		[XmlConfigFile(Category = "VCProjectFileGenerator")]
		public string ExcludedFilePaths = "";

		/// <summary>
		/// Whether to write a solution option (suo) file for the sln.
		/// </summary>
		[XmlConfigFile(Category = "BuildConfiguration")]
		public bool bWriteSolutionOptionFile = true;

		/// <summary>
		/// Whether to write a .vsconfig file next to the sln to suggest components to install.
		/// </summary>
		[XmlConfigFile(Category = "BuildConfiguration")]
		public bool bVsConfigFile = true;

		/// <summary>
		/// Forces UBT to be built in debug configuration, regardless of the solution configuration
		/// </summary>
		[XmlConfigFile(Category = "VCProjectFileGenerator")]
		public bool bBuildUBTInDebug = false;

		/// <summary>
		/// Whether to add the -FastPDB option to build command lines by default.
		/// </summary>
		[XmlConfigFile(Category = "BuildConfiguration")]
		public bool bAddFastPDBToProjects = false;

		/// <summary>
		/// Whether to generate per-file intellisense data.
		/// </summary>
		[XmlConfigFile(Category = "BuildConfiguration")]
		public bool bUsePerFileIntellisense = true;

		/// <summary>
		/// Whether to include a dependency on ShaderCompileWorker when generating project files for the editor.
		/// </summary>
		[XmlConfigFile(Category = "BuildConfiguration")]
		public bool bEditorDependsOnShaderCompileWorker = true;

		/// <summary>
		/// Whether to include a dependency on LiveCodingConsole when building targets that support live coding.
		/// </summary>
		[XmlConfigFile(Category = "VCProjectFileGenerator")]
		public bool bBuildLiveCodingConsole = false;
	}

	/// <summary>
	/// Visual C++ project file generator implementation
	/// </summary>
	class VCProjectFileGenerator : ProjectFileGenerator
	{
		/// <summary>
		/// The settings object
		/// </summary>
		protected VCProjectFileSettings Settings = new VCProjectFileSettings();

		/// <summary>
		/// Override for the build tool to use in generated projects. If the compiler version is specified on the command line, we use the same argument on the 
		/// command line for generated projects.
		/// </summary>
		string? BuildToolOverride;

		/// <summary>
		/// Default constructor
		/// </summary>
		/// <param name="InOnlyGameProject">The single project to generate project files for, or null</param>
		/// <param name="InProjectFileFormat">Override the project file format to use</param>
		/// <param name="InArguments">Additional command line arguments</param>
		public VCProjectFileGenerator(FileReference? InOnlyGameProject, VCProjectFileFormat InProjectFileFormat, CommandLineArguments InArguments)
			: base(InOnlyGameProject)
		{
			XmlConfig.ApplyTo(Settings);

			if (InProjectFileFormat != VCProjectFileFormat.Default)
			{
				Settings.ProjectFileFormat = InProjectFileFormat;
			}

			if (InArguments.HasOption("-2019"))
			{
				BuildToolOverride = "-2019";
			}
			else if (InArguments.HasOption("-2022"))
			{
				BuildToolOverride = "-2022";
			}

			// Allow generating the solution even if the only installed toolchain is banned.			
			MicrosoftPlatformSDK.IgnoreToolchainErrors = true;
		}

		public override string[] GetTargetArguments(string[] Arguments)
		{
			return Arguments.Where(s => string.Equals(s, BuildToolOverride, StringComparison.InvariantCultureIgnoreCase)).ToArray();
		}

		/// File extension for project files we'll be generating (e.g. ".vcxproj")
		override public string ProjectFileExtension
		{
			get
			{
				return ".vcxproj";
			}
		}

		/// <summary>
		/// </summary>
		public override void CleanProjectFiles(DirectoryReference InPrimaryProjectDirectory, string InPrimaryProjectName, DirectoryReference InIntermediateProjectFilesDirectory, ILogger Logger)
		{
			FileReference PrimaryProjectFile = FileReference.Combine(InPrimaryProjectDirectory, InPrimaryProjectName);
			FileReference PrimaryProjDeleteFilename = PrimaryProjectFile + ".sln";
			if (FileReference.Exists(PrimaryProjDeleteFilename))
			{
				FileReference.Delete(PrimaryProjDeleteFilename);
			}
			PrimaryProjDeleteFilename = PrimaryProjectFile + ".sdf";
			if (FileReference.Exists(PrimaryProjDeleteFilename))
			{
				FileReference.Delete(PrimaryProjDeleteFilename);
			}
			PrimaryProjDeleteFilename = PrimaryProjectFile + ".suo";
			if (FileReference.Exists(PrimaryProjDeleteFilename))
			{
				FileReference.Delete(PrimaryProjDeleteFilename);
			}
			PrimaryProjDeleteFilename = PrimaryProjectFile + ".v11.suo";
			if (FileReference.Exists(PrimaryProjDeleteFilename))
			{
				FileReference.Delete(PrimaryProjDeleteFilename);
			}
			PrimaryProjDeleteFilename = PrimaryProjectFile + ".v12.suo";
			if (FileReference.Exists(PrimaryProjDeleteFilename))
			{
				FileReference.Delete(PrimaryProjDeleteFilename);
			}
			PrimaryProjDeleteFilename = FileReference.Combine(InPrimaryProjectDirectory, ".vsconfig");
			if (FileReference.Exists(PrimaryProjDeleteFilename))
			{
				FileReference.Delete(PrimaryProjDeleteFilename);
			}

			// Delete the project files folder
			if (DirectoryReference.Exists(InIntermediateProjectFilesDirectory))
			{
				try
				{
					DirectoryReference.Delete(InIntermediateProjectFilesDirectory, true);
				}
				catch (Exception Ex)
				{
					Logger.LogInformation("Error while trying to clean project files path {InIntermediateProjectFilesDirectory}. Ignored.", InIntermediateProjectFilesDirectory);
					Logger.LogInformation("\t{Message}", Ex.Message);
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
			return new VCProjectFile(InitFilePath, BaseDir, Settings.ProjectFileFormat, bUsePrecompiled, BuildToolOverride, Settings);
		}


		/// "4.0", "12.0", or "14.0", etc...
		static public string GetProjectFileToolVersionString(VCProjectFileFormat ProjectFileFormat)
		{
			switch (ProjectFileFormat)
			{
				case VCProjectFileFormat.VisualStudio2019:
					return "16.0";
				case VCProjectFileFormat.VisualStudio2022:
					return "17.0";
			}
			return string.Empty;
		}

		/// for instance: <PlatformToolset>v110</PlatformToolset>
		static public string GetProjectFilePlatformToolsetVersionString(VCProjectFileFormat ProjectFileFormat)
		{
			switch (ProjectFileFormat)
			{
				case VCProjectFileFormat.VisualStudio2019:
					return "v142";
				case VCProjectFileFormat.VisualStudio2022:
					return "v143";

			}
			return string.Empty;
		}

		static public void AppendPlatformToolsetProperty(StringBuilder VCProjectFileContent, VCProjectFileFormat ProjectFileFormat)
		{
			string ToolVersionString = GetProjectFileToolVersionString(ProjectFileFormat);
			string PlatformToolsetVersionString = GetProjectFilePlatformToolsetVersionString(ProjectFileFormat);
			VCProjectFileContent.AppendLine("    <PlatformToolset>{0}</PlatformToolset>", PlatformToolsetVersionString);
		}

		/// <inheritdoc/>
		protected override void ConfigureProjectFileGeneration(String[] Arguments, ref bool IncludeAllPlatforms, ILogger Logger)
		{
			// Call parent implementation first
			base.ConfigureProjectFileGeneration(Arguments, ref IncludeAllPlatforms, Logger);
		}

		/// <summary>
		/// Selects which platforms and build configurations we want in the project file
		/// </summary>
		/// <param name="IncludeAllPlatforms">True if we should include ALL platforms that are supported on this machine.  Otherwise, only desktop platforms will be included.</param>
		/// <param name="Logger"></param>
		/// <param name="SupportedPlatformNames">Output string for supported platforms, returned as comma-separated values.</param>
		protected override void SetupSupportedPlatformsAndConfigurations(bool IncludeAllPlatforms, ILogger Logger, out string SupportedPlatformNames)
		{
			// Call parent implementation to figure out the actual platforms
			base.SetupSupportedPlatformsAndConfigurations(IncludeAllPlatforms, Logger, out SupportedPlatformNames);

			// If we have a non-default setting for visual studio, check the compiler exists. If not, revert to the default.
			if (Settings.ProjectFileFormat == VCProjectFileFormat.VisualStudio2022)
			{
				if (!WindowsPlatform.HasCompiler(WindowsCompiler.VisualStudio2022, UnrealArch.X64, Logger))
				{
					Logger.LogWarning("Visual Studio C++ 2022 installation not found - ignoring preferred project file format.");
					Settings.ProjectFileFormat = VCProjectFileFormat.Default;
				}
			}
			else if (Settings.ProjectFileFormat == VCProjectFileFormat.VisualStudio2019)
			{
				if (!WindowsPlatform.HasCompiler(WindowsCompiler.VisualStudio2019, UnrealArch.X64, Logger))
				{
					Logger.LogWarning("Visual Studio C++ 2019 installation not found - ignoring preferred project file format.");
					Settings.ProjectFileFormat = VCProjectFileFormat.Default;
				}
			}

			// Certain platforms override the project file format because their debugger add-ins may not yet support the latest
			// version of Visual Studio.  This is their chance to override that.
			// ...but only if the user didn't override this via the command-line.
			if (Settings.ProjectFileFormat == VCProjectFileFormat.Default)
			{
				// Enumerate all the valid installations. This list is already sorted by preference.
				List<VisualStudioInstallation> Installations = MicrosoftPlatformSDK.FindVisualStudioInstallations(Logger).Where(x => WindowsPlatform.HasCompiler(x.Compiler, UnrealArch.X64, Logger)).ToList();

				// Get the corresponding project file format
				VCProjectFileFormat Format = VCProjectFileFormat.Default;
				foreach (VisualStudioInstallation Installation in Installations)
				{
					if (Installation.Compiler == WindowsCompiler.VisualStudio2022)
					{
						Format = VCProjectFileFormat.VisualStudio2022;
						break;
					}
					else if (Installation.Compiler == WindowsCompiler.VisualStudio2019)
					{
						Format = VCProjectFileFormat.VisualStudio2019;
						break;
					}
				}
				Settings.ProjectFileFormat = Format;

				bool Vs2019Available = Installations.Any(x => x.Compiler == WindowsCompiler.VisualStudio2019);

				// Allow the SDKs to override
				foreach (UnrealTargetPlatform SupportedPlatform in SupportedPlatforms)
				{
					UEBuildPlatform? BuildPlatform;
					if (UEBuildPlatform.TryGetBuildPlatform(SupportedPlatform, out BuildPlatform))
					{
						// Don't worry about platforms that we're missing SDKs for
						if (BuildPlatform.HasRequiredSDKsInstalled() == SDKStatus.Valid)
						{
							VCProjectFileFormat ProposedFormat = BuildPlatform.GetRequiredVisualStudioVersion();

							if (ProposedFormat != VCProjectFileFormat.Default)
							{
								// Reduce the Visual Studio version to the max supported by each platform we plan to include.
								if (Settings.ProjectFileFormat == VCProjectFileFormat.Default || ProposedFormat < Settings.ProjectFileFormat)
								{
									Logger.LogInformation("Available {SupportedPlatform} SDK does not support Visual Studio 2022.", SupportedPlatform);
									Version Version = BuildPlatform.GetVersionRequiredForVisualStudio(VCProjectFileFormat.VisualStudio2022);
									if (Version > new Version())
									{
										Logger.LogInformation("Please update {SupportedPlatform} SDK to {Version} if Visual Studio 2022 support is desired.", SupportedPlatform, Version);
									}
									if (!Vs2019Available)
									{
										Logger.LogInformation("Generated solution cannot be downgraded to Visual Studio 2019 as it is not installed. Please install Visual Studio 2019 if {SupportedPlatform} SDK support is required.", SupportedPlatform);
									}
									else
									{
										Settings.ProjectFileFormat = ProposedFormat;
										Logger.LogInformation("Downgrading generated solution to Visual Studio 2019.");
										Logger.LogInformation("To force Visual Studio 2022 solutions to always be generated add the following to BuildConfiguration.xml:");
										Logger.LogInformation("  <VCProjectFileGenerator>\r\n    <Version>VisualStudio2022</Version>\r\n  </VCProjectFileGenerator>");
									}
									Logger.LogInformation(string.Empty);
								}
							}
						}
					}
				}
			}

			if (bIncludeDotNetPrograms && Settings.ProjectFileFormat == VCProjectFileFormat.VisualStudio2019)
			{
				Logger.LogInformation("Visual Studio 2019 does not support .NET 6.0 C# projects, these projects will not be added to the generated solution.");
				Logger.LogInformation("Please generate the Visual Studio 2022 solution if .NET 6.0 C# project support is required.");
				bIncludeDotNetPrograms = false;
			}
		}


		/// <summary>
		/// Used to sort VC solution config names along with the config and platform values
		/// </summary>
		public class VCSolutionConfigCombination
		{
			/// <summary>
			/// Visual Studio solution configuration name for this config+platform
			/// </summary>
			public string VCSolutionConfigAndPlatformName;

			/// <summary>
			/// Configuration name
			/// </summary>
			public UnrealTargetConfiguration Configuration;

			/// <summary>
			/// Platform name
			/// </summary>
			public UnrealTargetPlatform Platform;

			/// <summary>
			/// The target type
			/// </summary>
			public TargetType TargetConfigurationName;

			public override string ToString()
			{
				return String.Format("{0}={1} {2} {3}", VCSolutionConfigAndPlatformName, Configuration, Platform, TargetConfigurationName);
			}

			public VCSolutionConfigCombination(string VCSolutionConfigAndPlatformName)
			{
				this.VCSolutionConfigAndPlatformName = VCSolutionConfigAndPlatformName;
			}
		}


		/// <summary>
		/// Composes a string to use for the Visual Studio solution configuration, given a build configuration and target rules configuration name
		/// </summary>
		/// <param name="Configuration">The build configuration</param>
		/// <param name="TargetType">The type of target being built</param>
		/// <returns>The generated solution configuration name</returns>
		static string MakeSolutionConfigurationName(UnrealTargetConfiguration Configuration, TargetType TargetType)
		{
			string SolutionConfigName = Configuration.ToString();

			// Don't bother postfixing "Game" or "Program" -- that will be the default when using "Debug", "Development", etc.
			// Also don't postfix "RocketGame" when we're building Rocket game projects.  That's the only type of game there is in that case!
			if (TargetType != TargetType.Game && TargetType != TargetType.Program)
			{
				SolutionConfigName += " " + TargetType.ToString();
			}

			return SolutionConfigName;
		}

		static IDictionary<PrimaryProjectFolder, Guid> GenerateProjectFolderGuids(PrimaryProjectFolder RootFolder)
		{
			IDictionary<PrimaryProjectFolder, Guid> Guids = new Dictionary<PrimaryProjectFolder, Guid>();
			foreach (PrimaryProjectFolder Folder in RootFolder.SubFolders)
			{
				GenerateProjectFolderGuids("UE5", Folder, Guids);
			}
			return Guids;
		}

		static void GenerateProjectFolderGuids(string ParentPath, PrimaryProjectFolder Folder, IDictionary<PrimaryProjectFolder, Guid> Guids)
		{
			string Path = String.Format("{0}/{1}", ParentPath, Folder.FolderName);
			Guids[Folder] = MakeMd5Guid(Encoding.UTF8.GetBytes(Path));

			foreach (PrimaryProjectFolder SubFolder in Folder.SubFolders)
			{
				GenerateProjectFolderGuids(Path, SubFolder, Guids);
			}
		}

		static Guid MakeMd5Guid(byte[] Input)
		{
			byte[] Hash = MD5.Create().ComputeHash(Input);
			Hash[6] = (byte)(0x30 | (Hash[6] & 0x0f)); // 0b0011'xxxx Version 3 UUID (MD5)
			Hash[8] = (byte)(0x80 | (Hash[8] & 0x3f)); // 0b10xx'xxxx RFC 4122 UUID
			Array.Reverse(Hash, 0, 4);
			Array.Reverse(Hash, 4, 2);
			Array.Reverse(Hash, 6, 2);
			return new Guid(Hash);
		}

		public static Guid MakeMd5Guid(Guid Namespace, string Text)
		{
			byte[] Input = new byte[16 + Encoding.UTF8.GetByteCount(Text)];

			Namespace.TryWriteBytes(Input.AsSpan(0, 16));
			Array.Reverse(Input, 0, 4);
			Array.Reverse(Input, 4, 2);
			Array.Reverse(Input, 6, 2);

			Encoding.UTF8.GetBytes(Text, 0, Text.Length, Input, 16);
			return MakeMd5Guid(Input);
		}

		public static Guid MakeMd5Guid(string Text)
		{
			byte[] Input = Encoding.UTF8.GetBytes(Text);

			return MakeMd5Guid(Input);
		}

		/// <summary>
		/// Writes the project files to disk
		/// </summary>
		/// <returns>True if successful</returns>
		protected override bool WriteProjectFiles(PlatformProjectGeneratorCollection PlatformProjectGenerators, ILogger Logger)
		{
			if (!base.WriteProjectFiles(PlatformProjectGenerators, Logger))
			{
				return false;
			}

			// Write AutomationReferences file
			// Write in in net core expected format
			if (AutomationProjectFiles.Any())
			{
				XNamespace NS = XNamespace.Get("http://schemas.microsoft.com/developer/msbuild/2003");

				DirectoryReference AutomationToolDir = DirectoryReference.Combine(Unreal.EngineSourceDirectory, "Programs", "AutomationTool");
				new XDocument(
					new XElement(NS + "Project",
						new XAttribute("ToolsVersion", VCProjectFileGenerator.GetProjectFileToolVersionString(Settings.ProjectFileFormat)),
						new XAttribute("DefaultTargets", "Build"),
						new XElement(NS + "ItemGroup",
							from AutomationProject in AutomationProjectFiles
							select new XElement(NS + "ProjectReference",
								new XAttribute("Include", AutomationProject.ProjectFilePath.MakeRelativeTo(AutomationToolDir))
							)
						)
					)
				).Save(FileReference.Combine(IntermediateProjectFilesPath, "AutomationTool.csproj.References").FullName);
			}

			return true;
		}


		protected override bool WritePrimaryProjectFile(ProjectFile? UBTProject, PlatformProjectGeneratorCollection PlatformProjectGenerators, ILogger Logger)
		{
			bool bSuccess = true;

			string SolutionFileName = PrimaryProjectName + ".sln";

			// Setup solution file content
			StringBuilder VCSolutionFileContent = new StringBuilder();

			// Solution file header. Note that a leading newline is required for file type detection to work correclty in the shell.
			if (Settings.ProjectFileFormat == VCProjectFileFormat.VisualStudio2022)
			{
				VCSolutionFileContent.AppendLine();
				VCSolutionFileContent.AppendLine("Microsoft Visual Studio Solution File, Format Version 12.00");
				VCSolutionFileContent.AppendLine("# Visual Studio Version 17");
				VCSolutionFileContent.AppendLine("VisualStudioVersion = 17.0.31314.256");
				VCSolutionFileContent.AppendLine("MinimumVisualStudioVersion = 10.0.40219.1");
			}
			else if (Settings.ProjectFileFormat == VCProjectFileFormat.VisualStudio2019)
			{
				VCSolutionFileContent.AppendLine();
				VCSolutionFileContent.AppendLine("Microsoft Visual Studio Solution File, Format Version 12.00");
				VCSolutionFileContent.AppendLine("# Visual Studio Version 16");
				VCSolutionFileContent.AppendLine("VisualStudioVersion = 16.0.28315.86");
				VCSolutionFileContent.AppendLine("MinimumVisualStudioVersion = 10.0.40219.1");
			}
			else
			{
				throw new BuildException("Unexpected ProjectFileFormat");
			}

			IDictionary<PrimaryProjectFolder, Guid> ProjectFolderGuids = GenerateProjectFolderGuids(RootFolder);

			// check if the give folder has any projects that will be put into the solution
			System.Func<PrimaryProjectFolder, bool>? HasProjectFunc = null;
			HasProjectFunc = (ProjectFolder) =>
			{
				foreach (MSBuildProjectFile ChildProject in ProjectFolder.ChildProjects)
				{
					if (AllProjectFiles.Contains(ChildProject))
					{
						return true;
					}
				}

				foreach (PrimaryProjectFolder SubFolder in ProjectFolder.SubFolders)
				{
					if (HasProjectFunc!(SubFolder))
					{
						return true;
					}
				}
				return false;
			};

			// Solution folders, files and project entries
			{
				// This the GUID that Visual Studio uses to identify a solution folder
				string SolutionFolderEntryGUID = "{2150E333-8FDC-42A3-9474-1A3956D46DE8}";

				// Solution folders
				{
					IEnumerable<PrimaryProjectFolder> AllSolutionFolders = ProjectFolderGuids.Keys.OrderBy(Folder => Folder.FolderName).ThenBy(Folder => ProjectFolderGuids[Folder]);
					foreach (PrimaryProjectFolder CurFolder in AllSolutionFolders)
					{
						// if this folder has no projects anywhere under it then skip it
						if (!HasProjectFunc(CurFolder))
						{
							continue;
						}
						string FolderGUIDString = ProjectFolderGuids[CurFolder].ToString("B").ToUpperInvariant();
						VCSolutionFileContent.AppendLine("Project(\"" + SolutionFolderEntryGUID + "\") = \"" + CurFolder.FolderName + "\", \"" + CurFolder.FolderName + "\", \"" + FolderGUIDString + "\"");

						// Add any files that are inlined right inside the solution folder
						if (CurFolder.Files.Count > 0)
						{
							VCSolutionFileContent.AppendLine("	ProjectSection(SolutionItems) = preProject");
							foreach (string CurFile in CurFolder.Files)
							{
								// Syntax is:  <relative file path> = <relative file path>
								VCSolutionFileContent.AppendLine("		" + CurFile + " = " + CurFile);
							}
							VCSolutionFileContent.AppendLine("	EndProjectSection");
						}

						VCSolutionFileContent.AppendLine("EndProject");
					}
				}


				// Project files
				//List<MSBuildProjectFile> AllProjectFilesSorted = AllProjectFiles.OrderBy((ProjFile) => ProjFile.ProjectFilePath.GetFileNameWithoutExtension()).Cast<MSBuildProjectFile>().ToList();
				foreach (MSBuildProjectFile CurProject in AllProjectFiles)
				{
					// Visual Studio uses different GUID types depending on the project type
					string ProjectTypeGUID = CurProject.ProjectTypeGUID;

					// NOTE: The project name in the solution doesn't actually *have* to match the project file name on disk.  However,
					//       we prefer it when it does match so we use the actual file name here.
					string ProjectNameInSolution = CurProject.ProjectFilePath.GetFileNameWithoutExtension();

					// Use the existing project's GUID that's already known to us
					string ProjectGUID = CurProject.ProjectGUID.ToString("B").ToUpperInvariant();

					VCSolutionFileContent.AppendLine("Project(\"" + ProjectTypeGUID + "\") = \"" + ProjectNameInSolution + "\", \"" + CurProject.ProjectFilePath.MakeRelativeTo(ProjectFileGenerator.PrimaryProjectPath) + "\", \"" + ProjectGUID + "\"");

					// Setup dependency on UnrealBuildTool, if we need that.  This makes sure that UnrealBuildTool is
					// freshly compiled before kicking off any build operations on this target project
					if (!CurProject.IsStubProject)
					{
						List<ProjectFile> Dependencies = new List<ProjectFile>();
						if (CurProject.IsGeneratedProject && UBTProject != null && CurProject != UBTProject)
						{
							Dependencies.Add(UBTProject);
							Dependencies.AddRange(UBTProject.DependsOnProjects);
						}
						Dependencies.AddRange(CurProject.DependsOnProjects);

						if (Dependencies.Count > 0)
						{
							VCSolutionFileContent.AppendLine("\tProjectSection(ProjectDependencies) = postProject");

							// Setup any addition dependencies this project has...
							foreach (ProjectFile DependsOnProject in Dependencies)
							{
								string DependsOnProjectGUID = ((MSBuildProjectFile)DependsOnProject).ProjectGUID.ToString("B").ToUpperInvariant();
								VCSolutionFileContent.AppendLine("\t\t" + DependsOnProjectGUID + " = " + DependsOnProjectGUID);
							}

							VCSolutionFileContent.AppendLine("\tEndProjectSection");
						}
					}

					VCSolutionFileContent.AppendLine("EndProject");
				}

				// Get the path to the visualizers file. Try to make it relative to the solution directory, but fall back to a full path if it's a foreign project.
				FileReference VisualizersFile = FileReference.Combine(Unreal.EngineDirectory, "Extras", "VisualStudioDebugging", "Unreal.natvis");

				// Add the visualizers at the solution level. Doesn't seem to be picked up from a makefile project in VS2017 15.8.5.
				VCSolutionFileContent.AppendLine(String.Format("Project(\"{0}\") = \"Visualizers\", \"Visualizers\", \"{{1CCEC849-CC72-4C59-8C36-2F7C38706D4C}}\"", SolutionFolderEntryGUID));
				VCSolutionFileContent.AppendLine("\tProjectSection(SolutionItems) = preProject");
				VCSolutionFileContent.AppendLine("\t\t{0} = {0}", VisualizersFile.MakeRelativeTo(PrimaryProjectPath));
				VCSolutionFileContent.AppendLine("\tEndProjectSection");
				VCSolutionFileContent.AppendLine("EndProject");
			}

			// Solution configuration platforms.  This is just a list of all of the platforms and configurations that
			// appear in Visual Studio's build configuration selector.
			List<VCSolutionConfigCombination> SolutionConfigCombinations;

			// The "Global" section has source control, solution configurations, project configurations,
			// preferences, and project hierarchy data
			{
				VCSolutionFileContent.AppendLine("Global");
				{
					HashSet<UnrealTargetPlatform> PlatformsValidForProjects;
					{
						VCSolutionFileContent.AppendLine("	GlobalSection(SolutionConfigurationPlatforms) = preSolution");

						CollectSolutionConfigurations(SupportedConfigurations, SupportedPlatforms, AllProjectFiles,
							Logger, out PlatformsValidForProjects, out SolutionConfigCombinations);

						HashSet<string> AppendedSolutionConfigAndPlatformNames = new HashSet<string>(StringComparer.InvariantCultureIgnoreCase);
						foreach (VCSolutionConfigCombination SolutionConfigCombination in SolutionConfigCombinations)
						{
							// We alias "Game" and "Program" to both have the same solution configuration, so we're careful not to add the same combination twice.
							if (!AppendedSolutionConfigAndPlatformNames.Contains(SolutionConfigCombination.VCSolutionConfigAndPlatformName))
							{
								VCSolutionFileContent.AppendLine("		" + SolutionConfigCombination.VCSolutionConfigAndPlatformName + " = " + SolutionConfigCombination.VCSolutionConfigAndPlatformName);
								AppendedSolutionConfigAndPlatformNames.Add(SolutionConfigCombination.VCSolutionConfigAndPlatformName);
							}
						}

						VCSolutionFileContent.AppendLine("	EndGlobalSection");
					}

					// Embed UnrealVS section, which is parsed by that VSPackage (Extension) to know how to handle this solution
					{
						string UnrealVSGuid = "ddbf523f-7eb6-4887-bd51-85a714ff87eb";
						VCSolutionFileContent.AppendLine("\t# UnrealVS Section");
						VCSolutionFileContent.AppendLine("\tGlobalSection({0}) = preSolution", UnrealVSGuid);
						VCSolutionFileContent.AppendLine("\t\tAvailablePlatforms={0}", string.Join(";", PlatformsValidForProjects.Select(platform => platform.ToString())));
						VCSolutionFileContent.AppendLine("\tEndGlobalSection");
					}

					// Assign each project's "project configuration" to our "solution platform + configuration" pairs.  This
					// also sets up which projects are actually built when building the solution.
					{
						VCSolutionFileContent.AppendLine("	GlobalSection(ProjectConfigurationPlatforms) = postSolution");

						foreach (MSBuildProjectFile CurProject in AllProjectFiles)
						{
							foreach (VCSolutionConfigCombination SolutionConfigCombination in SolutionConfigCombinations)
							{
								// Get the context for the current solution context
								MSBuildProjectContext ProjectContext = CurProject.GetMatchingProjectContext(SolutionConfigCombination.TargetConfigurationName, SolutionConfigCombination.Configuration, SolutionConfigCombination.Platform, PlatformProjectGenerators, Logger);

								// Override the configuration to build for UBT
								if (Settings.bBuildUBTInDebug && CurProject == UBTProject)
								{
									ProjectContext.ConfigurationName = "Debug";
								}

								// Write the solution mapping (e.g.  "{4232C52C-680F-4850-8855-DC39419B5E9B}.Debug|iOS.ActiveCfg = iOS_Debug|Win32")
								string CurProjectGUID = CurProject.ProjectGUID.ToString("B").ToUpperInvariant();
								VCSolutionFileContent.AppendLine("		{0}.{1}.ActiveCfg = {2}", CurProjectGUID, SolutionConfigCombination.VCSolutionConfigAndPlatformName, ProjectContext.Name);
								if (ProjectContext.bBuildByDefault)
								{
									VCSolutionFileContent.AppendLine("		{0}.{1}.Build.0 = {2}", CurProjectGUID, SolutionConfigCombination.VCSolutionConfigAndPlatformName, ProjectContext.Name);
									if (ProjectContext.bDeployByDefault)
									{
										VCSolutionFileContent.AppendLine("		{0}.{1}.Deploy.0 = {2}", CurProjectGUID, SolutionConfigCombination.VCSolutionConfigAndPlatformName, ProjectContext.Name);
									}
								}
							}
						}

						VCSolutionFileContent.AppendLine("	EndGlobalSection");
					}


					// Setup other solution properties
					{
						// HideSolutionNode sets whether or not the top-level solution entry is completely hidden in the UI.
						// We don't want that, as we need users to be able to right click on the solution tree item.
						VCSolutionFileContent.AppendLine("	GlobalSection(SolutionProperties) = preSolution");
						VCSolutionFileContent.AppendLine("		HideSolutionNode = FALSE");
						VCSolutionFileContent.AppendLine("	EndGlobalSection");
					}



					// Solution directory hierarchy
					{
						VCSolutionFileContent.AppendLine("	GlobalSection(NestedProjects) = preSolution");

						// Every entry in this section is in the format "Guid1 = Guid2".  Guid1 is the child project (or solution
						// filter)'s GUID, and Guid2 is the solution filter directory to parent the child project (or solution
						// filter) to.  This sets up the hierarchical solution explorer tree for all solution folders and projects.

						System.Action<StringBuilder /* VCSolutionFileContent */, List<PrimaryProjectFolder> /* Folders */ >? FolderProcessorFunction = null;
						FolderProcessorFunction = (LocalVCSolutionFileContent, LocalPrimaryProjectFolders) =>
							{
								foreach (PrimaryProjectFolder CurFolder in LocalPrimaryProjectFolders)
								{
									string CurFolderGUIDString = ProjectFolderGuids[CurFolder].ToString("B").ToUpperInvariant();

									foreach (MSBuildProjectFile ChildProject in CurFolder.ChildProjects)
									{
										if (AllProjectFiles.Contains(ChildProject))
										{
											//	e.g. "{BF6FB09F-A2A6-468F-BE6F-DEBE07EAD3EA} = {C43B6BB5-3EF0-4784-B896-4099753BCDA9}"
											LocalVCSolutionFileContent.AppendLine("		" + ChildProject.ProjectGUID.ToString("B").ToUpperInvariant() + " = " + CurFolderGUIDString);
										}
									}

									foreach (PrimaryProjectFolder SubFolder in CurFolder.SubFolders)
									{
										// if this folder has no projects anywhere under it then skip it
										if (HasProjectFunc(SubFolder))
										{
											//	e.g. "{BF6FB09F-A2A6-468F-BE6F-DEBE07EAD3EA} = {C43B6BB5-3EF0-4784-B896-4099753BCDA9}"
											LocalVCSolutionFileContent.AppendLine("		" + ProjectFolderGuids[SubFolder].ToString("B").ToUpperInvariant() + " = " + CurFolderGUIDString);
										}
									}

									// Recurse into subfolders
									FolderProcessorFunction!(LocalVCSolutionFileContent, CurFolder.SubFolders);
								}
							};
						FolderProcessorFunction(VCSolutionFileContent, RootFolder.SubFolders);

						VCSolutionFileContent.AppendLine("	EndGlobalSection");
					}
				}

				VCSolutionFileContent.AppendLine("EndGlobal");
			}


			// Save the solution file
			if (bSuccess)
			{
				string SolutionFilePath = FileReference.Combine(PrimaryProjectPath, SolutionFileName).FullName;
				bSuccess = WriteFileIfChanged(SolutionFilePath, VCSolutionFileContent.ToString(), Logger);
			}


			// Save a solution config file which selects the development editor configuration by default.
			// .suo file writable only on Windows, requires ole32
			if (bSuccess && Settings.bWriteSolutionOptionFile && OperatingSystem.IsWindows())
			{
				// Figure out the filename for the SUO file. VS will automatically import the options from earlier versions if necessary.
				FileReference SolutionOptionsFileName;
				switch (Settings.ProjectFileFormat)
				{
					case VCProjectFileFormat.VisualStudio2019:
						SolutionOptionsFileName = FileReference.Combine(PrimaryProjectPath, ".vs", Path.GetFileNameWithoutExtension(SolutionFileName), "v16", ".suo");
						break;
					case VCProjectFileFormat.VisualStudio2022:
						SolutionOptionsFileName = FileReference.Combine(PrimaryProjectPath, ".vs", Path.GetFileNameWithoutExtension(SolutionFileName), "v17", ".suo");
						break;
					default:
						throw new BuildException("Unsupported Visual Studio version");
				}

				// Check it doesn't exist before overwriting it. Since these files store the user's preferences, it'd be bad form to overwrite them.
				if (!FileReference.Exists(SolutionOptionsFileName))
				{
					DirectoryReference.CreateDirectory(SolutionOptionsFileName.Directory);

					VCSolutionOptions Options = new VCSolutionOptions(Settings.ProjectFileFormat);

					// Set the default configuration and startup project
					VCSolutionConfigCombination? DefaultConfig = SolutionConfigCombinations.Find(x => x.Configuration == UnrealTargetConfiguration.Development && x.Platform == UnrealTargetPlatform.Win64 && x.TargetConfigurationName == TargetType.Editor);
					if (DefaultConfig != null)
					{
						List<VCBinarySetting> Settings = new List<VCBinarySetting>();
						Settings.Add(new VCBinarySetting("ActiveCfg", DefaultConfig.VCSolutionConfigAndPlatformName));
						if (DefaultProject != null)
						{
							Settings.Add(new VCBinarySetting("StartupProject", ((MSBuildProjectFile)DefaultProject).ProjectGUID.ToString("B")));
						}
						Options.SetConfiguration(Settings);
					}

					// Mark all the projects as closed by default, apart from the startup project
					VCSolutionExplorerState ExplorerState = new VCSolutionExplorerState();
					if (Settings.ProjectFileFormat >= VCProjectFileFormat.VisualStudio2019)
					{
						BuildSolutionExplorerState_VS2019(RootFolder, "", ExplorerState, DefaultProject);
					}
					Options.SetExplorerState(ExplorerState);

					// Write the file
					if (Options.Sections.Count > 0)
					{
						Options.Write(SolutionOptionsFileName.FullName);
					}
				}
			}

			if (bSuccess && Settings.bVsConfigFile && OperatingSystem.IsWindows())
			{
				StringBuilder VsConfigFileContent = new StringBuilder();

				VsConfigFileContent.AppendLine("{");
				VsConfigFileContent.AppendLine("  \"version\": \"1.0\",");
				VsConfigFileContent.AppendLine("  \"components\": [");
				IEnumerable<string> Components = MicrosoftPlatformSDK.GetVisualStudioSuggestedComponents(Settings.ProjectFileFormat);
				string ComponentsString = string.Join($",{Environment.NewLine}    ", Components.Select(x => $"\"{x}\""));
				VsConfigFileContent.AppendLine($"    {ComponentsString}");
				VsConfigFileContent.AppendLine("  ]");
				VsConfigFileContent.AppendLine("}");

				FileReference VsConfigFileName = FileReference.Combine(PrimaryProjectPath, ".vsconfig");
				bSuccess = WriteFileIfChanged(VsConfigFileName.FullName, VsConfigFileContent.ToString(), Logger);
			}

			return bSuccess;
		}

		public static void CollectSolutionConfigurations(List<UnrealTargetConfiguration> AllConfigurations,
			List<UnrealTargetPlatform> AllPlatforms, List<ProjectFile> AllProjectFiles, ILogger Logger,
			out HashSet<UnrealTargetPlatform> OutValidPlatforms, out List<VCSolutionConfigCombination> OutSolutionConfigs)
		{
			OutValidPlatforms = new HashSet<UnrealTargetPlatform>();
			OutSolutionConfigs = new List<VCSolutionConfigCombination>();
			Dictionary<string, Tuple<UnrealTargetConfiguration, TargetType>> SolutionConfigurationsValidForProjects =
				new Dictionary<string, Tuple<UnrealTargetConfiguration, TargetType>>();

			foreach (UnrealTargetConfiguration CurConfiguration in AllConfigurations)
			{
				if (InstalledPlatformInfo.IsValidConfiguration(CurConfiguration, EProjectType.Code))
				{
					foreach (UnrealTargetPlatform CurPlatform in AllPlatforms)
					{
						if (InstalledPlatformInfo.IsValidPlatform(CurPlatform, EProjectType.Code))
						{
							foreach (ProjectFile CurProject in AllProjectFiles)
							{
								if (!CurProject.IsStubProject)
								{
									if (CurProject.ProjectTargets.Count == 0)
									{
										throw new BuildException("Expecting project '" + CurProject.ProjectFilePath +
										                         "' to have at least one ProjectTarget associated with it!");
									}

									// Figure out the set of valid target configuration names
									foreach (Project ProjectTarget in CurProject.ProjectTargets)
									{
										if (VCProjectFile.IsValidProjectPlatformAndConfiguration(ProjectTarget, CurPlatform,
											    CurConfiguration, Logger))
										{
											OutValidPlatforms.Add(CurPlatform);

											// Default to a target configuration name of "Game", since that will collapse down to an empty string
											TargetType TargetType = TargetType.Game;
											if (ProjectTarget.TargetRules != null)
											{
												TargetType = ProjectTarget.TargetRules.Type;
											}

											string SolutionConfigName =
												MakeSolutionConfigurationName(CurConfiguration, TargetType);
											SolutionConfigurationsValidForProjects[SolutionConfigName] =
												new Tuple<UnrealTargetConfiguration, TargetType>(CurConfiguration, TargetType);
										}
									}
								}
							}
						}
					}
				}
			}

			foreach (UnrealTargetPlatform CurPlatform in OutValidPlatforms)
			{
				foreach (KeyValuePair<string, Tuple<UnrealTargetConfiguration, TargetType>> SolutionConfigKeyValue in
				         SolutionConfigurationsValidForProjects)
				{
					// e.g.  "Development|Win64 = Development|Win64"
					string SolutionConfigName = SolutionConfigKeyValue.Key;
					UnrealTargetConfiguration Configuration = SolutionConfigKeyValue.Value.Item1;
					TargetType TargetType = SolutionConfigKeyValue.Value.Item2;

					string SolutionPlatformName = CurPlatform.ToString();

					string SolutionConfigAndPlatformPair = SolutionConfigName + "|" + SolutionPlatformName;
					OutSolutionConfigs.Add(
						new VCSolutionConfigCombination(SolutionConfigAndPlatformPair)
						{
							Configuration = Configuration,
							Platform = CurPlatform,
							TargetConfigurationName = TargetType
						}
					);
				}
			}

			// Sort the list of solution platform strings alphabetically (Visual Studio prefers it)
			OutSolutionConfigs.Sort(
				new Comparison<VCSolutionConfigCombination>(
					(x, y) =>
					{
						return String.Compare(x.VCSolutionConfigAndPlatformName, y.VCSolutionConfigAndPlatformName,
							StringComparison.InvariantCultureIgnoreCase);
					}
				)
			);
		}

		protected override void WriteDebugSolutionFiles(PlatformProjectGeneratorCollection PlatformProjectGenerators, DirectoryReference IntermediateProjectFilesPath, ILogger Logger)
		{
			//build and collect UnrealVS configuration
			StringBuilder UnrealVSContent = new StringBuilder();
			foreach (UnrealTargetPlatform SupportedPlatform in SupportedPlatforms)
			{
				PlatformProjectGenerator? ProjGenerator = PlatformProjectGenerators.GetPlatformProjectGenerator(SupportedPlatform, true);
				if (ProjGenerator != null)
				{
					ProjGenerator.GetUnrealVSConfigurationEntries(UnrealVSContent);
				}
			}
			if (UnrealVSContent.Length > 0)
			{
				UnrealVSContent.Insert(0, "<UnrealVS>" + ProjectFileGenerator.NewLine);
				UnrealVSContent.Append("</UnrealVS>" + ProjectFileGenerator.NewLine);

				string ConfigFilePath = FileReference.Combine(IntermediateProjectFilesPath, "UnrealVS.xml").FullName;
				/* bool bSuccess = */
				ProjectFileGenerator.WriteFileIfChanged(ConfigFilePath, UnrealVSContent.ToString(), Logger);
			}
		}

		static void BuildSolutionExplorerState_VS2019(PrimaryProjectFolder Folder, string Suffix, VCSolutionExplorerState ExplorerState, ProjectFile? DefaultProject)
		{
			foreach (ProjectFile Project in Folder.ChildProjects)
			{
				string ProjectIdentifier = String.Format("{0}{1}", Project.ProjectFilePath.GetFileNameWithoutExtension(), Suffix);
				if (Project == DefaultProject)
				{
					ExplorerState.OpenProjects.Add(new Tuple<string, string[]>(ProjectIdentifier, new string[] { ProjectIdentifier }));
				}
				else
				{
					ExplorerState.OpenProjects.Add(new Tuple<string, string[]>(ProjectIdentifier, new string[] { }));
				}
			}

			foreach (PrimaryProjectFolder SubFolder in Folder.SubFolders)
			{
				string SubFolderName = SubFolder.FolderName + Suffix;
				if (SubFolderName == "Automation;Programs")
				{
					ExplorerState.OpenProjects.Add(new Tuple<string, string[]>(SubFolderName, new string[] { }));
				}
				else
				{
					ExplorerState.OpenProjects.Add(new Tuple<string, string[]>(SubFolderName, new string[] { SubFolderName }));
				}
				BuildSolutionExplorerState_VS2019(SubFolder, ";" + SubFolderName, ExplorerState, DefaultProject);
			}
		}

		/// <summary>
		/// Takes a string and "cleans it up" to make it parsable by the Visual Studio source control provider's file format
		/// </summary>
		/// <param name="Str">String to clean up</param>
		/// <returns>The cleaned up string</returns>
		public string CleanupStringForSCC(string Str)
		{
			string Cleaned = Str;

			// SCC is expecting paths to contain only double-backslashes for path separators.  It's a bit weird but we need to do it.
			Cleaned = Cleaned.Replace(Path.DirectorySeparatorChar.ToString(), Path.DirectorySeparatorChar.ToString() + Path.DirectorySeparatorChar.ToString());
			Cleaned = Cleaned.Replace(Path.AltDirectorySeparatorChar.ToString(), Path.DirectorySeparatorChar.ToString() + Path.DirectorySeparatorChar.ToString());

			// SCC is expecting not to see spaces in these strings, so we'll replace spaces with "\u0020"
			Cleaned = Cleaned.Replace(" ", "\\u0020");

			return Cleaned;
		}

	}

}
