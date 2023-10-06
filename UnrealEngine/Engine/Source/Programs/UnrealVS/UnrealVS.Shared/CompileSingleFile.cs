// Copyright Epic Games, Inc. All Rights Reserved.

using EnvDTE;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio.Shell.Interop;
using System;
using System.Collections.Generic;
using System.ComponentModel.Design;
using System.Diagnostics;
using System.IO;
using System.Windows.Forms;
using System.Text;
using System.Linq;

namespace UnrealVS
{
	class CompileSingleFile : IDisposable
	{
		private const int CompileSingleFileButtonID = 0x1075;
		private const int PreprocessSingleFileButtonID = 0x1076;
		private const int CompileSingleModuleButtonID = 0x1077;
		private const int CompileAndProfileSingleFileButtonID = 0x1078;
		private const int GenerateAssemblyFileButtonID = 0x1079;
		private const int UBTSubMenuID = 0x3103;
		private string	  FileToCompileOriginalExt = "";

		static readonly HashSet<string> ValidExtensions = new HashSet<string>(StringComparer.OrdinalIgnoreCase) { ".c", ".cc", ".cpp", ".h", ".cxx", ".ispc" };

		System.Diagnostics.Process ChildProcess;
		private OleMenuCommand SubMenuCommand;

		public CompileSingleFile()
		{
			CommandID CommandID = new CommandID(GuidList.UnrealVSCmdSet, CompileSingleFileButtonID);
			var CompileSingleFileButtonCommand = new OleMenuCommand(new EventHandler(CompileSingleFileButtonHandler), CommandID);
			CompileSingleFileButtonCommand.BeforeQueryStatus += CompileSingleFileButtonCommand_BeforeQueryStatus;
			UnrealVSPackage.Instance.MenuCommandService.AddCommand(CompileSingleFileButtonCommand);

			CommandID CommandID2 = new CommandID(GuidList.UnrealVSCmdSet, PreprocessSingleFileButtonID);
			var PreprocessSingleFileButtonCommand = new OleMenuCommand(new EventHandler(CompileSingleFileButtonHandler), CommandID2);
			PreprocessSingleFileButtonCommand.BeforeQueryStatus += CompileSingleFileButtonCommand_BeforeQueryStatus;
			UnrealVSPackage.Instance.MenuCommandService.AddCommand(PreprocessSingleFileButtonCommand);

			CommandID CommandID3 = new CommandID(GuidList.UnrealVSCmdSet, CompileSingleModuleButtonID);
			MenuCommand CompileSingleModuleButtonCommand = new MenuCommand(new EventHandler(CompileSingleFileButtonHandler), CommandID3);
			UnrealVSPackage.Instance.MenuCommandService.AddCommand(CompileSingleModuleButtonCommand);

			CommandID CommandID4 = new CommandID(GuidList.UnrealVSCmdSet, CompileAndProfileSingleFileButtonID);
			MenuCommand CompileAndProfileSingleFileButtonCommand = new MenuCommand(new EventHandler(CompileSingleFileButtonHandler), CommandID4);
			UnrealVSPackage.Instance.MenuCommandService.AddCommand(CompileAndProfileSingleFileButtonCommand);

			CommandID CommandID5 = new CommandID(GuidList.UnrealVSCmdSet, GenerateAssemblyFileButtonID);
			MenuCommand GenerateAssemblyFileButtonCommand = new MenuCommand(new EventHandler(CompileSingleFileButtonHandler), CommandID5);
			UnrealVSPackage.Instance.MenuCommandService.AddCommand(GenerateAssemblyFileButtonCommand);

			// add sub menu for UBT commands
			SubMenuCommand = new OleMenuCommand(null, new CommandID(GuidList.UnrealVSCmdSet, UBTSubMenuID));
			UnrealVSPackage.Instance.MenuCommandService.AddCommand(SubMenuCommand);
		}

		private void CompileSingleFileButtonCommand_BeforeQueryStatus(object sender, EventArgs e)
		{
			ThreadHelper.ThrowIfNotOnUIThread();
			DTE DTE = UnrealVSPackage.Instance.DTE;
			MenuCommand MenuCommand = sender as MenuCommand;
			if (MenuCommand == null)
			{
				return;
			}

			if (DTE?.ActiveDocument == null)
			{
				MenuCommand.Enabled = false;
				return;
			}

			// Check if the requested file is valid
			string FileToCompileExt = Path.GetExtension(DTE.ActiveDocument.FullName);
			MenuCommand.Enabled = ValidExtensions.Contains(FileToCompileExt);
		}

		public void Dispose()
		{
			KillChildProcess();
		}

		void CompileSingleFileButtonHandler(object Sender, EventArgs Args)
		{
			ThreadHelper.ThrowIfNotOnUIThread();

			MenuCommand SenderSubMenuCommand = (MenuCommand)Sender;

			bool bIsFile = SenderSubMenuCommand.CommandID.ID != CompileSingleModuleButtonID;
			bool bPreprocessOnly = SenderSubMenuCommand.CommandID.ID == PreprocessSingleFileButtonID;
			bool bProfile = SenderSubMenuCommand.CommandID.ID == CompileAndProfileSingleFileButtonID;
			bool bGenerateAssembly = SenderSubMenuCommand.CommandID.ID == GenerateAssemblyFileButtonID;

			if (!TryCompileSingleFileOrModule(bIsFile, bPreprocessOnly, bProfile, bGenerateAssembly))
			{
				DTE DTE = UnrealVSPackage.Instance.DTE;
				DTE.ExecuteCommand("Build.Compile");
			}
		}

		void KillChildProcess()
		{
			if (ChildProcess != null)
			{
				if (!ChildProcess.HasExited)
				{
					ChildProcess.Kill();
					ChildProcess.WaitForExit();
				}
				ChildProcess.Dispose();
				ChildProcess = null;
			}
		}

		string FindModuleForFile(string fileName, string rootDirectory)
		{
			string directory = Path.GetDirectoryName(fileName);
			IEnumerable<string> buildFiles = Directory.EnumerateFiles(directory, "*.build.cs", SearchOption.TopDirectoryOnly);
			string buildFile = buildFiles.FirstOrDefault();
			if (buildFile != null)
			{
				return Path.GetFileName(buildFile.Substring(0, buildFile.LastIndexOf(".build.cs", StringComparison.OrdinalIgnoreCase)));
			}
			if (string.Equals(directory, rootDirectory, StringComparison.OrdinalIgnoreCase))
			{
				return null;
			}
			return FindModuleForFile(directory, rootDirectory);
		}

		bool TryCompileSingleFileOrModule(bool bIsFile, bool bPreProcessOnly, bool bProfile, bool bGenerateAssembly)
		{
			ThreadHelper.ThrowIfNotOnUIThread();
			DTE DTE = UnrealVSPackage.Instance.DTE;

			// Activate the output window
			Window Window = DTE.Windows.Item(EnvDTE.Constants.vsWindowKindOutput);
			Window.Activate();

			// Find or create the 'Build' window
			IVsOutputWindowPane BuildOutputPane = UnrealVSPackage.Instance.GetOutputPane();
			if (BuildOutputPane == null)
			{
				Logging.WriteLine("CompileSingleFile: Build Output Pane not found");
				return false;
			}

			// If there's already a build in progress, offer to cancel it
			if (ChildProcess != null && !ChildProcess.HasExited)
			{
				if (MessageBox.Show("Cancel current compile?", "Compile in progress", MessageBoxButtons.YesNo) == DialogResult.Yes)
				{
					KillChildProcess();
					BuildOutputPane.OutputStringThreadSafe($"1>  Build cancelled.{Environment.NewLine}");
				}
				return true;
			}

			// Check we've got a file open
			if (DTE.ActiveDocument == null)
			{
				Logging.WriteLine("CompileSingleFile: ActiveDocument not found");
				return false;
			}

			// Grab the current startup project
			UnrealVSPackage.Instance.SolutionBuildManager.get_StartupProject(out IVsHierarchy ProjectHierarchy);
			if (ProjectHierarchy == null)
			{
				Logging.WriteLine("CompileSingleFile: ProjectHierarchy not found");
				return false;
			}
			Project StartupProject = Utils.HierarchyObjectToProject(ProjectHierarchy);
			if (StartupProject == null)
			{
				Logging.WriteLine("CompileSingleFile: StartupProject not found");
				return false;
			}
			if (!(StartupProject.Object is Microsoft.VisualStudio.VCProjectEngine.VCProject VCStartupProject))
			{
				Logging.WriteLine("CompileSingleFile: VCStartupProject not found");
				return false;
			}

			// Get the active configuration for the startup project
			Configuration ActiveConfiguration = StartupProject.ConfigurationManager.ActiveConfiguration;
			string ActiveConfigurationName = $"{ActiveConfiguration.ConfigurationName}|{ActiveConfiguration.PlatformName}";
			Microsoft.VisualStudio.VCProjectEngine.VCConfiguration ActiveVCConfiguration = (VCStartupProject.Configurations as Microsoft.VisualStudio.VCProjectEngine.IVCCollection).Item(ActiveConfigurationName) as Microsoft.VisualStudio.VCProjectEngine.VCConfiguration;
			if (ActiveVCConfiguration == null)
			{
				Logging.WriteLine("CompileSingleFile: VCStartupProject ActiveConfiguration not found");
				return false;
			}

			// Get the NMake settings for this configuration
			Microsoft.VisualStudio.VCProjectEngine.VCNMakeTool ActiveNMakeTool = (ActiveVCConfiguration.Tools as Microsoft.VisualStudio.VCProjectEngine.IVCCollection).Item("VCNMakeTool") as Microsoft.VisualStudio.VCProjectEngine.VCNMakeTool;
			if (ActiveNMakeTool == null)
			{
				MessageBox.Show($"No NMakeTool set for Project {VCStartupProject.Name} set for single-file compile.", "UnrealVS - NMakeTool not set", MessageBoxButtons.OK);
				return false;
			}

			// Save all the open documents
			DTE.ExecuteCommand("File.SaveAll");

			// Check if the requested file is valid
			string FileToCompile = DTE.ActiveDocument.FullName;
			string FileToCompileExt = Path.GetExtension(FileToCompile);

			string CompilingText;
			string UBTArgument;
			if (bIsFile)
			{
				if (!ValidExtensions.Contains(FileToCompileExt.ToLowerInvariant()))
				{
					MessageBox.Show($"Invalid file extension {FileToCompileExt} for single-file compile.", "Invalid Extension", MessageBoxButtons.OK);
					return true;
				}

				CompilingText = FileToCompile;
				UBTArgument = $"-singlefile=\"{FileToCompile}";
			}
			else
			{
				string ModuleName = FindModuleForFile(FileToCompile, Path.GetDirectoryName(DTE.Solution.FileName));
				if (ModuleName == null)
				{
					MessageBox.Show($"Can't find module for for {FileToCompile} to compile.", "Invalid Module", MessageBoxButtons.OK);
				}
				CompilingText = ModuleName;
				UBTArgument = $"-Module=\"{ModuleName}";
			}

			// If there's already a build in progress, don't let another one start
			if (DTE.Solution.SolutionBuild.BuildState == vsBuildState.vsBuildStateInProgress)
			{
				if (MessageBox.Show("Cancel current compile?", "Compile in progress", MessageBoxButtons.YesNo) == DialogResult.Yes)
				{
					DTE.ExecuteCommand("Build.Cancel");
				}
				return true;
			}

			// Make sure any existing build is stopped
			KillChildProcess();

			// Set up the output pane
			BuildOutputPane.Activate();
			BuildOutputPane.Clear();
			BuildOutputPane.OutputStringThreadSafe($"1>------ Build started: Project: {StartupProject.Name}, Configuration: {ActiveConfiguration.ConfigurationName} {ActiveConfiguration.PlatformName} ------{Environment.NewLine}");
			BuildOutputPane.OutputStringThreadSafe($"1>  Compiling {CompilingText}{Environment.NewLine}");

			// Set up event handlers 
			DTE.Events.BuildEvents.OnBuildBegin += BuildEvents_OnBuildBegin;

			// Create a delegate for handling output messages
			string PPLine = String.Empty;
			void OutputHandler(object Sender, DataReceivedEventArgs Args) 
			{ 
				if (Args.Data != null) 
				{ 
					BuildOutputPane.OutputStringThreadSafe($"1>  {Args.Data}{Environment.NewLine}"); 
					if (Args.Data.Contains("PreProcessPath:"))
					{
						PPLine = Args.Data;
					}
				} 
			}

			if (bProfile)
			{
				var Commands = DTE.Commands.Cast<Command>();
				#pragma warning disable VSTHRD010
				Command Command = Commands.FirstOrDefault((C) => C.Name == "CompileScore.StartTrace");
				#pragma warning restore VSTHRD010
				if (Command == null || !Command.IsAvailable)
				{
					MessageBox.Show($"CompileSingleFileAndProfile requires a CompileScore visual studio extension that has CompileScore.StartTrace/CompileScore.StopTrace installed", "UnrealVS - Missing CompileScore extension ", MessageBoxButtons.OK);
					return true;
				}
				Object CustomIn = null;
				Object CustomOut = null;
				DTE.Commands.Raise(Command.Guid, Command.ID, ref CustomIn, ref CustomOut);
				//DTE.ExecuteCommand("CompileScore.StartTrace");
			}

			string SolutionDir = Path.GetDirectoryName(UnrealVSPackage.Instance.SolutionFilepath).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
			// Get the build command line and escape any environment variables that we use
			string BuildCommandLine = ActiveNMakeTool.BuildCommandLine;
			BuildCommandLine = BuildCommandLine.Replace("$(SolutionDir)", SolutionDir);
			BuildCommandLine = BuildCommandLine.Replace("$(ProjectName)", VCStartupProject.Name);
			BuildCommandLine = BuildCommandLine.Replace("$(BuildBatchScript)", "..\\..\\Build\\BatchFiles\\Build.bat");

			FileToCompileOriginalExt = FileToCompileExt;

			string ExtraArgs = "";

			if (bPreProcessOnly)
			{
				ExtraArgs = " -NoXGE -Preprocess ";
			}
			else if (bGenerateAssembly)
			{
				ExtraArgs = " -NOXGE -WithAssembly ";
			}

			// Spawn the new process
			ChildProcess = new System.Diagnostics.Process();
			ChildProcess.StartInfo.FileName = Path.Combine(Environment.SystemDirectory, "cmd.exe");
			ChildProcess.StartInfo.Arguments = $"/C \"{BuildCommandLine} {ExtraArgs} {UBTArgument}\"\"";
			ChildProcess.StartInfo.WorkingDirectory = Path.GetDirectoryName(StartupProject.FullName);
			ChildProcess.StartInfo.UseShellExecute = false;
			ChildProcess.StartInfo.RedirectStandardOutput = true;
			ChildProcess.StartInfo.RedirectStandardError = true;
			ChildProcess.StartInfo.CreateNoWindow = true;
			ChildProcess.OutputDataReceived += OutputHandler;
			ChildProcess.ErrorDataReceived += OutputHandler;
			if (bPreProcessOnly || bProfile)
			{
				// add an event handler to respond to the exit of the preprocess request
				// and open the generated file if it exists.
				ChildProcess.EnableRaisingEvents = true;
				ChildProcess.Exited += new EventHandler((s, e) => PreprocessExitHandler(PPLine, bProfile));
			}
			
			ChildProcess.Start();
			ChildProcess.BeginOutputReadLine();
			ChildProcess.BeginErrorReadLine();

			return true;
		}

		private void PreprocessExitHandler(string PPLine, bool bIsProfiling)
		{
			if (bIsProfiling)
			{
				ThreadHelper.JoinableTaskFactory.Run(async () =>
				{
					await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
					UnrealVSPackage.Instance.DTE.ExecuteCommand("CompileScore.StopTrace");
				});
			}

			// not all compile actions support pre-process - check it exists
			if (PPLine.Contains("PreProcessPath:"))
			{
				string PPFullPath = PPLine.Replace("PreProcessPath:", "").Trim();

				ThreadHelper.JoinableTaskFactory.Run(async () =>
				{
					await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
					OpenPreprocessedFile(PPFullPath);
				});
			}
		}

		private void OpenPreprocessedFile(string PPFullPath)
		{
			ThreadHelper.ThrowIfNotOnUIThread();

			IVsOutputWindowPane BuildOutputPane = UnrealVSPackage.Instance.GetOutputPane();
			DTE DTE = UnrealVSPackage.Instance.DTE;

			BuildOutputPane.OutputStringThreadSafe($"1>  PPFullPath: {PPFullPath}{Environment.NewLine}");

			if (File.Exists(PPFullPath))
			{
				// if the file exists, rename it to isolate the file and have its extension be the original to maintain syntax highlighting
				string Dir = Path.GetDirectoryName(PPFullPath);
				string FileName = Path.GetFileNameWithoutExtension(Path.GetFileNameWithoutExtension(PPFullPath)) + "_preprocessed";

				string RenamedFile = Path.Combine(Dir, FileName) + FileToCompileOriginalExt;

				File.Copy(PPFullPath, RenamedFile, true /*overwrite*/);

				DTE.ExecuteCommand("File.OpenFile", $"\"{RenamedFile}\"");
			}
		}

		private void BuildEvents_OnBuildBegin(vsBuildScope Scope, vsBuildAction Action)
		{
			KillChildProcess();
		}
	}
}
