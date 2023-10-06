// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	class KDevelopProjectFile : ProjectFile
	{
		public KDevelopProjectFile(FileReference InitFilePath, DirectoryReference BaseDir)
			: base(InitFilePath, BaseDir)
		{
		}
	}

	/// <summary>
	/// KDevelop project file generator implementation
	/// </summary>
	class KDevelopGenerator : ProjectFileGenerator
	{
		public KDevelopGenerator(FileReference? InOnlyGameProject)
			: base(InOnlyGameProject)
		{
		}

		/// File extension for project files we'll be generating (e.g. ".vcxproj")
		public override string ProjectFileExtension => ".kdev4";

		protected override bool WritePrimaryProjectFile(ProjectFile? UBTProject, PlatformProjectGeneratorCollection PlatformProjectGenerators, ILogger Logger)
		{
			bool bSuccess = true;
			return bSuccess;
		}

		/// <summary>
		/// Write the primare $ProjectName.kdev4 project file, the one that should be opened when loading the project
		/// into KDevelop
		/// </summary>
		/// <param name="FileContent">File content.</param>
		/// <param name="Name">Name.</param>
		private void WriteKDevPrimaryProjectSection(ref StringBuilder FileContent, string Name)
		{
			FileContent.Append("\n");
			FileContent.Append("[Project] \n");
			FileContent.Append("Manager=KDevCustomBuildSystem \n");
			FileContent.Append("Name=");
			FileContent.Append(Name);
			FileContent.Append("\n");
		}

		/// <summary>
		/// Write the Command Sub section for .kdev4/$ProjectName.kdev4 file
		/// </summary>
		/// <param name="FileContent">File content.</param>
		/// <param name="TargetName">Target name.</param>
		/// <param name="ConfName">Conf name.</param>
		/// <param name="BuildConfigIndex">Build config index.</param>
		/// <param name="Type">Type.</param>
		private void WriteCommandSubSection(ref StringBuilder FileContent, string TargetName, string ConfName, int BuildConfigIndex, int Type)
		{
			string ToolType = "";
			string Executable = "bash";
			string ProjectCmdArg = "";
			string BuildCommand = "Engine/Build/BatchFiles/Linux/Build.sh";

			if (TargetName == GameProjectName)
			{
				ProjectCmdArg = " -project=\"" + OnlyGameProject!.FullName + "\"";

				if (Type == 1)
				{
					ProjectCmdArg = " -makefile -kdevelopfile " + ProjectCmdArg + " -game -engine ";
				}
			}
			else if (TargetName == (GameProjectName + "Editor"))
			{
				ProjectCmdArg = " -editorrecompile -project=\"" + OnlyGameProject!.FullName + "\"";

				if (Type == 1)
				{
					ProjectCmdArg = " -makefile -kdevelopfile " + ProjectCmdArg + " -game -engine ";
				}
			}
			else
			{
				if (Type == 1)
				{
					// Override BuildCommand and ProjectCmdArg
					BuildCommand = "./GenerateProjectFiles.sh";
					// ProjectCmdArg = "";
				}
			}

			if (Type == 0)
			{
				ToolType = "Build";
			}
			else if (Type == 1)
			{
				ToolType = "Configure";
			}
			else if (Type == 3)
			{
				ToolType = "Clean";
				ConfName = ConfName + " -clean";
			}

			FileContent.Append(String.Format("[CustomBuildSystem][BuildConfig{0}][Tool{1}]\n", BuildConfigIndex, ToolType));
			FileContent.Append(String.Format("Arguments={0} {1} {2} Linux {3}\n", BuildCommand, ProjectCmdArg, TargetName, ConfName));
			FileContent.Append("Enabled=true\n");
			FileContent.Append("Environment=\n");
			FileContent.Append(String.Format("Executable={0}\n", Executable));
			FileContent.Append(String.Format("Type={0}\n\n", Type));

		}

		/// <summary>
		///  Write the Command section for a .kdev4/$ProjectName.kdev4 file.
		/// </summary>
		/// <param name="FileContent">File content.</param>
		private void WriteCommandSection(ref StringBuilder FileContent)
		{
			int BuildConfigIndex = 1;

			string UnrealRootPath = Unreal.RootDirectory.FullName;
			FileContent.Append("[CustomBuildSystem]\n");
			FileContent.Append("CurrentConfiguration=BuildConfig0\n\n"); //

			// The Basics to get up and running with the editor, utilizing the Makefile.
			FileContent.Append(String.Format("[CustomBuildSystem][BuildConfig0]\nBuildDir=file://{0}\n", UnrealRootPath));

			FileContent.Append("Title=BuildMeFirst\n\n");
			FileContent.Append("[CustomBuildSystem][BuildConfig0][ToolBuild]\n");
			FileContent.Append("Arguments=-f Makefile UnrealEditor UnrealGame ShaderCompileWorker UnrealLightmass UnrealPak\n");
			FileContent.Append("Enabled=true\n");
			FileContent.Append("Environment=\n");
			FileContent.Append("Executable=make\n");
			FileContent.Append("Type=0\n\n");

			FileContent.Append("[CustomBuildSystem][BuildConfig0][ToolClean]\n");
			FileContent.Append("Arguments=-f Makefile UnrealEditor UnrealGame ShaderCompileWorker UnrealLightmass UnrealPak -clean\n");
			FileContent.Append("Enabled=true\n");
			FileContent.Append("Environment=\n");
			FileContent.Append("Executable=make\n");
			FileContent.Append("Type=3\n\n");

			FileContent.Append("[CustomBuildSystem][BuildConfig0][ToolConfigure]\n");
			FileContent.Append("Arguments=./GenerateProjectFiles.sh\n");
			FileContent.Append("Enabled=true\n");
			FileContent.Append("Environment=\n");
			FileContent.Append("Executable=bash\n");
			FileContent.Append("Type=1\n\n");

			foreach (ProjectFile Project in GeneratedProjectFiles)
			{
				foreach (ProjectTarget TargetFile in Project.ProjectTargets.OfType<ProjectTarget>())
				{
					if (TargetFile.TargetFilePath == null)
					{
						continue;
					}

					string TargetName = TargetFile.TargetFilePath.GetFileNameWithoutAnyExtensions();

					// Remove both ".cs" and ".
					foreach (UnrealTargetConfiguration CurConfiguration in (UnrealTargetConfiguration[])Enum.GetValues(typeof(UnrealTargetConfiguration)))
					{
						if (CurConfiguration != UnrealTargetConfiguration.Unknown && CurConfiguration != UnrealTargetConfiguration.Development)
						{
							if (InstalledPlatformInfo.IsValidConfiguration(CurConfiguration, EProjectType.Code))
							{
								string ConfName = Enum.GetName(typeof(UnrealTargetConfiguration), CurConfiguration)!;
								FileContent.Append(String.Format("[CustomBuildSystem][BuildConfig{0}]\nBuildDir=file://{1}\n", BuildConfigIndex, UnrealRootPath));

								if (TargetName == GameProjectName)
								{
									FileContent.Append(String.Format("Title={0}-Linux-{1}\n\n", TargetName, ConfName));
									WriteCommandSubSection(ref FileContent, TargetName, ConfName, BuildConfigIndex, 0);
									WriteCommandSubSection(ref FileContent, TargetName, ConfName, BuildConfigIndex, 1);
									WriteCommandSubSection(ref FileContent, TargetName, ConfName, BuildConfigIndex, 3);
								}
								else if (TargetName == (GameProjectName + "Editor"))
								{
									FileContent.Append(String.Format("Title={0}-Linux-{1}\n\n", TargetName, ConfName));
									WriteCommandSubSection(ref FileContent, TargetName, ConfName, BuildConfigIndex, 0);
									WriteCommandSubSection(ref FileContent, TargetName, ConfName, BuildConfigIndex, 1);
									WriteCommandSubSection(ref FileContent, TargetName, ConfName, BuildConfigIndex, 3);
								}
								else
								{
									FileContent.Append(String.Format("Title={0}-Linux-{1}\n\n", TargetName, ConfName));
									WriteCommandSubSection(ref FileContent, TargetName, ConfName, BuildConfigIndex, 0);
									WriteCommandSubSection(ref FileContent, TargetName, ConfName, BuildConfigIndex, 1);
									WriteCommandSubSection(ref FileContent, TargetName, ConfName, BuildConfigIndex, 3);
								}
								BuildConfigIndex++;
							}
						}
					}

					FileContent.Append(String.Format("[CustomBuildSystem][BuildConfig{0}]\nBuildDir=file://{1}\n", BuildConfigIndex, UnrealRootPath));
					if (TargetName == GameProjectName)
					{
						FileContent.Append(String.Format("Title={0}\n\n", TargetName));
						WriteCommandSubSection(ref FileContent, TargetName, "Development", BuildConfigIndex, 0);
						WriteCommandSubSection(ref FileContent, TargetName, "Development", BuildConfigIndex, 1);
						WriteCommandSubSection(ref FileContent, TargetName, "Development", BuildConfigIndex, 3);

					}
					else if (TargetName == (GameProjectName + "Editor"))
					{
						FileContent.Append(String.Format("Title={0}\n\n", TargetName));
						WriteCommandSubSection(ref FileContent, TargetName, "Development", BuildConfigIndex, 0);
						WriteCommandSubSection(ref FileContent, TargetName, "Development", BuildConfigIndex, 1);
						WriteCommandSubSection(ref FileContent, TargetName, "Development", BuildConfigIndex, 3);
					}
					else
					{
						FileContent.Append(String.Format("Title={0}\n\n", TargetName));
						WriteCommandSubSection(ref FileContent, TargetName, "Development", BuildConfigIndex, 0);
						WriteCommandSubSection(ref FileContent, TargetName, "Development", BuildConfigIndex, 1);
						WriteCommandSubSection(ref FileContent, TargetName, "Development", BuildConfigIndex, 3);
					}
					BuildConfigIndex++;
				}
			}
		}

		/// <summary>
		/// Adds the include directory to the list, after converting it to an absolute path to UnrealEngine root directory.
		/// </summary>
		/// <param name="FileContent">File content.</param>
		/// <param name="Logger"></param>
		private void WriteIncludeSection(ref StringBuilder FileContent, ILogger Logger)
		{
			List<string> IncludeDirectories = new List<string>();
			List<string> SystemIncludeDirectories = new List<string>();

			string UnrealEngineRootPath = Unreal.RootDirectory.FullName;

			int IncludeIndex = 1;
			// Iterate through all the include paths that
			// UnrealBuildTool generates

			foreach (ProjectFile CurProject in GeneratedProjectFiles)
			{
				KDevelopProjectFile? KDevelopProject = CurProject as KDevelopProjectFile;
				if (KDevelopProject == null)
				{
					Logger.LogInformation("KDevelopProject == null");
					continue;
				}

				foreach (string CurPath in KDevelopProject.IntelliSenseIncludeSearchPaths)
				{
					string FullProjectPath = ProjectFileGenerator.PrimaryProjectPath.FullName;
					string FullPath = "";

					// need to test to see if this in the project souce tree
					if (CurPath.StartsWith("/") && !CurPath.StartsWith(FullProjectPath))
					{
						// Full path to a folder outside of project
						FullPath = CurPath;
					}
					else
					{
						FullPath = Path.GetFullPath(Path.Combine(Path.GetDirectoryName(KDevelopProject.ProjectFilePath.FullName)!, CurPath));
						FullPath = Utils.MakePathRelativeTo(FullPath, FullProjectPath);
						FullPath = FullPath.TrimEnd('/');
						FullPath = Path.Combine(UnrealEngineRootPath, FullPath);

					}

					if (!FullPath.Contains("FortniteGame/") && !FullPath.Contains("ThirdParty/"))
					{
						SystemIncludeDirectories.Add(String.Format("{0}", FullPath));
						IncludeIndex++;
					}
				}

				foreach (string CurPath in KDevelopProject.IntelliSenseSystemIncludeSearchPaths)
				{
					string FullProjectPath = ProjectFileGenerator.PrimaryProjectPath.FullName;
					string FullPath = "";

					if (CurPath.StartsWith("/") && !CurPath.StartsWith(FullProjectPath))
					{
						// Full path to a folder outside of project
						FullPath = CurPath;
					}
					else
					{
						FullPath = Path.GetFullPath(Path.Combine(Path.GetDirectoryName(KDevelopProject.ProjectFilePath.FullName)!, CurPath));
						FullPath = Utils.MakePathRelativeTo(FullPath, FullProjectPath);
						FullPath = FullPath.TrimEnd('/');
						FullPath = Path.Combine(UnrealEngineRootPath, FullPath);
					}

					if (!FullPath.Contains("FortniteGame/") && !FullPath.Contains("ThirdParty/")) // @todo: skipping Fortnite header paths to shorten clang command line for building
					{
						SystemIncludeDirectories.Add(String.Format("{0}", FullPath));
						IncludeIndex++;
					}
				}
			}

			// Remove duplicate paths from include dir and system include dir list
			List<string> Tmp = new List<string>();
			List<string> Stmp = new List<string>();

			Tmp = IncludeDirectories.Distinct().ToList();
			Stmp = SystemIncludeDirectories.Distinct().ToList();

			IncludeDirectories = Tmp.ToList();
			SystemIncludeDirectories = Stmp.ToList();

			foreach (string CurPath in IncludeDirectories)
			{
				FileContent.Append(CurPath);
				FileContent.Append(" \n");
			}

			foreach (string CurPath in SystemIncludeDirectories)
			{
				FileContent.Append(CurPath);
				FileContent.Append(" \n");
			}
			FileContent.Append("\n");
		}

		/// <summary>
		/// Splits the definition text into macro name and value (if any).
		/// </summary>
		/// <param name="Definition">Definition text</param>
		/// <param name="Key">Out: The definition name</param>
		/// <param name="Value">Out: The definition value or null if it has none</param>
		/// <returns>Pair representing macro name and value.</returns>
		private void SplitDefinitionAndValue(string Definition, out String Key, out String Value)
		{
			int EqualsIndex = Definition.IndexOf('=');
			if (EqualsIndex >= 0)
			{
				Key = Definition.Substring(0, EqualsIndex);
				Value = Definition.Substring(EqualsIndex + 1);
			}
			else
			{
				Key = Definition;
				Value = "";
			}
		}

		/// <summary>
		/// Write the defines section to the .kdev4/$ProjectName.kdev4 project file.
		/// </summary>
		/// <param name="FileContent">File content.</param>
		/// <param name="Logger">Logger for output</param>
		private void WriteDefineSection(ref StringBuilder FileContent, ILogger Logger)
		{
			String Key = "";
			String Value = "";

			List<string> DefineHolder = new List<string>();

			foreach (ProjectFile CurProject in GeneratedProjectFiles)
			{
				KDevelopProjectFile? KDevelopProject = CurProject as KDevelopProjectFile;
				if (KDevelopProject == null)
				{
					Logger.LogInformation("KDevelopProject == null");
					continue;
				}

				foreach (string CurDefine in KDevelopProject.IntelliSensePreprocessorDefinitions)
				{
					SplitDefinitionAndValue(CurDefine, out Key, out Value);
					if (String.IsNullOrEmpty(Value))
					{
						DefineHolder.Add(String.Format("{0} \\\n", Key));
					}
					else
					{
						DefineHolder.Add(String.Format("{0}={1} \\\n", Key, Value));
					}
				}
			}

			// Remove duplicates if they are present.
			List<string> Tmp = new List<string>();
			Tmp = DefineHolder.Distinct().ToList();
			DefineHolder = Tmp.ToList();

			foreach (string Def in DefineHolder)
			{
				FileContent.Append(Def);
			}

			FileContent.Append("\n\n");
		}

		/// <summary>
		/// Excludes list in one big ugly list.
		/// </summary>
		/// <param name="FileContent">File content.</param>
		private void WriteExcludeSection(ref StringBuilder FileContent)
		{
			// Default excludes list with Engine/Intermediate, *PCH.h, and *.gch added to the exclude list
			FileContent.Append("[Filters]\nsize=30\n\n[Filters][0]\ninclusive=0\npattern=.*\ntargets=3\n\n" +
				"[Filters][1]\ninclusive=0\npattern=.git\ntargets=2\n\n[Filters][10]\ninclusive=0\npattern=*.o\ntargets=1\n\n" +
				"[Filters][11]\ninclusive=0\npattern=*.a\ntargets=1\n\n[Filters][12]\ninclusive=0\npattern=*.so\ntargets=1\n\n" +
				"[Filters][13]\ninclusive=0\npattern=*.so.*\ntargets=1\n\n[Filters][14]\ninclusive=0\npattern=moc_*.cpp\ntargets=1\n\n" +
				"[Filters][15]\ninclusive=0\npattern=*.moc\ntargets=1\n\n[Filters][16]\ninclusive=0\npattern=ui_*.h\ntargets=1\n\n" +
				"[Filters][17]\ninclusive=0\npattern=qrc_*.cpp\ntargets=1\n\n[Filters][18]\ninclusive=0\npattern=*~\ntargets=1\n\n" +
				"[Filters][19]\ninclusive=0\npattern=*.orig\ntargets=1\n\n[Filters][2]\ninclusive=0\npattern=CVS\ntargets=2\n\n" +
				"[Filters][20]\ninclusive=0\npattern=.*.kate-swp\ntargets=1\n\n[Filters][21]\ninclusive=0\npattern=.*.swp\ntargets=1\n\n" +
				"[Filters][22]\ninclusive=0\npattern=*.pyc\ntargets=1\n\n[Filters][23]\ninclusive=0\npattern=*.pyo\ntargets=1\n\n" +
				"[Filters][24]\ninclusive=0\npattern=Engine/Intermediate\ntargets=2\n\n[Filters][25]\ninclusive=0\npattern=*PCH.h\ntargets=1\n\n" +
				"[Filters][26]\ninclusive=0\npattern=*.gch\ntargets=1\n\n[Filters][27]\ninclusive=0\npattern=*.generated.h\ntargets=1\n\n" +
				"[Filters][28]\ninclusive=0\npattern=Intermediate\ntargets=2\n\n[Filters][3]\ninclusive=0\npattern=.svn\ntargets=2\n\n" +
				"[Filters][4]\ninclusive=0\npattern=_svn\ntargets=2\n\n[Filters][5]\ninclusive=0\npattern=SCCS\ntargets=2\n\n" +
				"[Filters][6]\ninclusive=0\npattern=_darcs\ntargets=2\n\n[Filters][7]\ninclusive=0\npattern=.hg\ntargets=2\n\n" +
				"[Filters][8]\ninclusive=0\npattern=.bzr\ntargets=2\n\n[Filters][9]\ninclusive=0\npattern=__pycache__\ntargets=2\n\n" +
				"[Filters][29]\ninclusive=0\npattern=Engine/Source/ThirdParty\ntargets=2\n\n");

		}

		/// Simple Place to call all the Write*Section functions.
		private bool WriteKDevelopPro(ILogger Logger)
		{
			// RAKE! Take one KDevelopProjectFileContent and pass
			// it through each function that writes out the sections.
			StringBuilder KDevelopFileContent = new StringBuilder();
			StringBuilder KDevelopPrimaryFileContent = new StringBuilder();

			// These are temp files until we can write them to the 
			// *.kdev4 filename directly 
			StringBuilder DefinesFileContent = new StringBuilder();
			StringBuilder IncludesFileContent = new StringBuilder();

			string FileName = PrimaryProjectName + ".kdev4";

			string DefinesFileName = "Defines.txt"; // RAKE! TEMP!
			string IncludesFileName = "Includes.txt"; // RAKE! TEMP!

			WriteKDevPrimaryProjectSection(ref KDevelopPrimaryFileContent, PrimaryProjectName);

			WriteCommandSection(ref KDevelopFileContent);
			WriteIncludeSection(ref IncludesFileContent, Logger);
			WriteDefineSection(ref DefinesFileContent, Logger);
			WriteExcludeSection(ref KDevelopFileContent);

			// Write the primary kdev file.
			string FullPrimaryProjectPath = Path.Combine(PrimaryProjectPath.FullName, ".kdev4/");

			if (!Directory.Exists(FullPrimaryProjectPath))
			{
				Directory.CreateDirectory(FullPrimaryProjectPath);
			}

			string FullKDevelopPrimaryFileName = Path.Combine(PrimaryProjectPath.FullName, FileName);
			string FullKDevelopFileName = Path.Combine(FullPrimaryProjectPath, FileName);

			string FullDefinesFileName = Path.Combine(FullPrimaryProjectPath, DefinesFileName);
			string FullIncludesFileName = Path.Combine(FullPrimaryProjectPath, IncludesFileName);

			WriteFileIfChanged(FullDefinesFileName, DefinesFileContent.ToString(), Logger);
			WriteFileIfChanged(FullIncludesFileName, IncludesFileContent.ToString(), Logger);

			return WriteFileIfChanged(FullKDevelopPrimaryFileName, KDevelopPrimaryFileContent.ToString(), Logger) &&
			WriteFileIfChanged(FullKDevelopFileName, KDevelopFileContent.ToString(), Logger);
		}

		/// ProjectFileGenerator interface
		//protected override bool WritePrimaryProjectFile( ProjectFile UBTProject )
		protected override bool WriteProjectFiles(PlatformProjectGeneratorCollection PlatformProjectGenerators, ILogger Logger)
		{
			return WriteKDevelopPro(Logger);
		}

		/// ProjectFileGenerator interface
		/// <summary>
		/// Allocates a generator-specific project file object
		/// </summary>
		/// <param name="InitFilePath">Path to the project file</param>
		/// <param name="BaseDir">The base directory for files within this project</param>
		/// <returns>The newly allocated project file object</returns>
		protected override ProjectFile AllocateProjectFile(FileReference InitFilePath, DirectoryReference BaseDir)
		{
			return new KDevelopProjectFile(InitFilePath, BaseDir);
		}

		/// ProjectFileGenerator interface
		public override void CleanProjectFiles(DirectoryReference InPrimaryProjectDirectory, string InPrimaryProjectName, DirectoryReference InIntermediateProjectFilesDirectory, ILogger Logger)
		{
		}
	}
}
