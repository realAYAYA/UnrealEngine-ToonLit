// Copyright Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using System.Linq;
using AutomationTool;
using UnrealBuildTool;
using EpicGames.Core;
using System.Text.RegularExpressions;
using System.Threading;
using UnrealBuildBase;
using Microsoft.Extensions.Logging;

abstract class SyncProjectBase : BuildCommand
{
	protected bool FindProjectFileAndP4Path(string InProjectArgument, out FileReference OutProjectFile, out string OutP4ProjectPath)
	{
		OutProjectFile = null;
		OutP4ProjectPath = null;

		OutProjectFile = ProjectUtils.FindProjectFileFromName(InProjectArgument);

		if (OutProjectFile != null)
		{
			// get the path in P4
			P4WhereRecord Record = GetP4RecordForPath(OutProjectFile.FullName);
			OutP4ProjectPath = Record.ClientFile;
		}
		else
		{
			// if they provided a name and not a path then find the file (requires that it's synced).
			string RelativePath = InProjectArgument;

			if (Path.GetExtension(RelativePath).Length == 0)
			{
				RelativePath = Path.ChangeExtension(RelativePath, "uproject");
			}

			if (!RelativePath.Contains(Path.DirectorySeparatorChar) && !RelativePath.Contains(Path.AltDirectorySeparatorChar))
			{
				RelativePath = CommandUtils.CombinePaths(PathSeparator.Slash, Path.GetFileNameWithoutExtension(RelativePath), RelativePath);
			}

			Logger.LogInformation("{InProjectArgument} not on disk. Searching P4 for {RelativePath}", InProjectArgument, RelativePath);

			List<string> SearchPaths = new List<string>();
			SearchPaths.Add("");
			string ProjectDirsFile = Directory.EnumerateFiles(CommandUtils.CombinePaths(CmdEnv.LocalRoot), "*.uprojectdirs").FirstOrDefault();
			if (ProjectDirsFile != null)
			{
				foreach (string FilePath in File.ReadAllLines(ProjectDirsFile))
				{
					string Trimmed = FilePath.Trim();
					if (!Trimmed.StartsWith("./", StringComparison.OrdinalIgnoreCase) &&
						!Trimmed.StartsWith(";", StringComparison.OrdinalIgnoreCase) &&
						Trimmed.IndexOfAny(Path.GetInvalidPathChars()) < 0)
					{
						SearchPaths.Add(Trimmed);
					}
				}
			}

			// Get the root of the branch containing the selected file
			foreach (string SearchPath in SearchPaths)
			{
				string P4Path = CommandUtils.CombinePaths(PathSeparator.Slash, P4Env.ClientRoot, SearchPath, RelativePath);

				if (P4.FileExistsInDepot(P4Path))
				{
					P4WhereRecord Record = GetP4RecordForPath(P4Path);
					// make sure to sync with //workspace/path as it cleans up files if the user has stream switched
					OutP4ProjectPath = Record.ClientFile;
					Logger.LogInformation("Found project at {OutP4ProjectPath}", OutP4ProjectPath);
					break;
				}
			}
		}

		Logger.LogDebug("Resolved {InProjectArgument} to P4 Path {OutP4ProjectPath}", InProjectArgument, OutP4ProjectPath);

		return OutP4ProjectPath != null && OutProjectFile != null;
	}

	protected P4WhereRecord GetP4RecordForPath(string FilePath)
	{
		return P4.Where(FilePath, AllowSpew: false).FirstOrDefault(x => x.DepotFile != null && !x.bUnmap);
	}
}

[Help("Syncs and builds all the binaries required for a project")]
[Help("project=<FortniteGame>", "Project to sync. Will search current path and paths in ueprojectdirs. If omitted will sync projectdirs")]
[Help("threads=N", "How many threads to use when syncing. Default=2. When >1 all output happens first")]
[Help("cl=<12345>", "Changelist to sync to. If omitted will sync to latest CL of the workspace path. 0 Will Remove files!")]
[Help("clean", "Clean old files before building")]
[Help("build", "Build after syncing")]
[Help("open", "Open project editor after syncing")]
[Help("generate", "Generate project files after syncing")]
[Help("force", "force sync files (files opened for edit will be untouched)")]
[Help("preview", "Shows commands that will be executed but performs no operations")]
[Help("projectonly", "Only sync the project")]
[Help("paths", "Only sync this path. Can be comma-separated list.")]
[Help("retries=n", "Number of retries for a timed out file. Defaults to 3")]
[Help("unversioned", "Do not set an engine version after syncing")]
[Help("path", "Only sync files that match this path. Can be comma-separated list.")]
[RequireP4]
[DoesNotNeedP4CL]
class SyncProject : SyncProjectBase
{
	public override ExitCode Execute()
	{
		if (!ParseParam("Deprecated"))
		{
			Logger.LogError("**************************************************************************");
			Logger.LogError("The SyncProject command has been deprecated, and will be removed in an");
			Logger.LogError("upcoming UnrealEngine release.");
			Logger.LogError("");
			Logger.LogError("Similar functionality is available through the UnrealGameSync command-line");
			Logger.LogError("tool, which is supported on Windows, Mac and Linux.");
			Logger.LogError("");
			Logger.LogError("To ignore this warning and continue to using SyncProject anyway, add the");
			Logger.LogError("-Deprecated argument to the command line.");
			Logger.LogError("**************************************************************************");
			return ExitCode.Error_Unknown;
		}

		Logger.LogInformation("************************* SyncProject");

		// These are files that should always be synced because tools update them
		string[] ForceSyncFiles = new string[]
		{
			"Engine/Build/Build.version",
			"Engine/Source/Programs/Shared/MetaData.cs"
		};

		// Parse the project filename (as a local path)
		string ProjectArg = ParseParamValue("Project", null);

		// Parse the changelist to sync to. -1 will sync to head.
		int CL = ParseParamInt("CL", -1);
		int Threads = ParseParamInt("threads", 2);
		bool ForceSync = ParseParam("force");
		bool PreviewOnly = ParseParam("preview");

		bool ProjectOnly = ParseParam("projectonly");

		int Retries = ParseParamInt("retries", 3);
		bool Unversioned = ParseParam("unversioned");

		bool BuildProject = ParseParam("build");
		bool OpenProject = ParseParam("open");
		bool GenerateProject = ParseParam("generate");
		
		string PathArg = ParseParamValue("paths", "");

		IEnumerable<string> ExplicitPaths = PathArg.Split(new char[] { ',',';' }, StringSplitOptions.RemoveEmptyEntries).Select(S => S.Trim());

		if (CL == 0)
		{
			if (!ForceSync)
			{
				throw new AutomationException("If using 0 CL to remove files -force must also be specified");
			}

			// Cannot unsync the engine folder
			ProjectOnly = true;
		}
		else if (CL == -1)
		{
			// find most recent change
			List<P4Connection.ChangeRecord> Records;

			string Cmd = string.Format("-s submitted -m1 //{0}/...", P4Env.Client);
			if (!P4.Changes(out Records, Cmd, false))
			{
				throw new AutomationException("p4 changes failed. Cannot find most recent CL");
			}

			CL = Records.First().CL;
		}

		bool EngineOnly = string.IsNullOrEmpty(ProjectArg);

		// Will be the full local path to the project file once resolved
		FileReference ProjectFile = null;

		// Will be the full workspace path to the project in P4 (if a project was specified)
		string P4ProjectPath = null;

		// Will be the full workspace path to the engine in P4 (If projectonly wasn't specified);
		string P4EnginePath = null;		

		// If we're syncing a project find where it is in P4
		if (!EngineOnly)
		{
			if (!FindProjectFileAndP4Path(ProjectArg, out ProjectFile, out P4ProjectPath))
			{
				throw new AutomationException("Could not find project file for {0} locally or in P4. Provide a full path or check the subdirectory is listed in UE4Games.uprojectdirs", ProjectArg);
			}			
		}

		// Build the list of paths that need syncing
		List<string> SyncPaths = new List<string>();
		
		// 
		if (ExplicitPaths.Any())
		{
			// Add all explicit paths as <root>/Path/...
			ExplicitPaths.ToList().ForEach(P => SyncPaths.Add(CommandUtils.CombinePaths(PathSeparator.Slash, P4Env.ClientRoot, P, "...")));
		}
		else
		{
			// See if the engine is in P4 too by checking the p4 location of a local file
			if (!ProjectOnly)
			{
				string LocalEngineFile = CommandUtils.CombinePaths(CmdEnv.LocalRoot, "Engine", "Source", "UnrealEditor.target.cs");
				P4WhereRecord EngineRecord = GetP4RecordForPath(LocalEngineFile);

				if (P4.FileExistsInDepot(EngineRecord.DepotFile))
				{
					// make sure to sync with //workspace/path as it cleans up files if the user has stream switched
					P4EnginePath = EngineRecord.ClientFile.Replace("Engine/Source/UnrealEditor.target.cs", "");
					SyncPaths.Add(CommandUtils.CombinePaths(PathSeparator.Slash, P4EnginePath + "*"));
					SyncPaths.Add(CommandUtils.CombinePaths(PathSeparator.Slash, P4EnginePath, "Engine", "..."));
				}
			}

			if (!string.IsNullOrEmpty(P4ProjectPath))
			{
				string P4ProjectDir = Regex.Replace(P4ProjectPath, @"[^/]+\.uproject", "...", RegexOptions.IgnoreCase);
				SyncPaths.Add(P4ProjectDir);
			}
		}
		
		// Force these files as they can be overwritten by tools
		if (!PreviewOnly && !string.IsNullOrEmpty(P4EnginePath) && !ProjectOnly)
		{
			IEnumerable<string> ForceSyncList = ForceSyncFiles.Select(F => CommandUtils.CombinePaths(PathSeparator.Slash, P4EnginePath, F));

			foreach (var F in ForceSyncList)
			{
				Logger.LogInformation("Force-updating {F}", F);

				string SyncCommand = string.Format("-f {0}@{1}", F, CL);

				// sync with retries
				P4.Sync(SyncCommand, true, false, Retries);
			}
		}

		// Sync all the things
		foreach(string SyncPath in SyncPaths)
		{
			string SyncCommand = "";

			if (Threads > 1)
			{
				SyncCommand = string.Format(" --parallel \"threads={0}\" {1}", Threads, SyncCommand);
			}

			if (ForceSync)
			{
				SyncCommand = SyncCommand + " -f";
			}

			SyncCommand += string.Format(" {0}@{1}", SyncPath, CL);

			if (!PreviewOnly)
			{
				// sync with retries
				P4.Sync(SyncCommand, true, false, Retries);
			}
			else
			{
				Logger.LogInformation("sync {SyncCommand}", SyncCommand);
			}
		}

		// P4 utils don't return errors :(
		ExitCode ExitStatus = ExitCode.Success;

		// Sync is complete so do the actions
		if (!PreviewOnly && CL > 0)
		{
			// Argument to pass to the editor (could be null with no project).
			string ProjectArgForEditor = "";
						
			if (!string.IsNullOrEmpty(ProjectArg))
			{
				// If we synced the project from P4 we couldn't resolve this earlier
				if (ProjectFile == null)
				{
					NativeProjects.ClearCache();
					ProjectFile = ProjectUtils.FindProjectFileFromName(ProjectArg);
				}

				ProjectArgForEditor = ProjectFile.FullName;
			}

			if (GenerateProject)
			{
				Logger.LogDebug("Generating project files for {ProjectArgForEditor}", ProjectArgForEditor);

				if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win64)
				{
					CommandUtils.Run("GenerateProjectFiles.bat", ProjectArgForEditor);
				}
				else
				{
					CommandUtils.Run("GenerateProjectFiles.sh", ProjectArgForEditor);
				}
			}

			UnrealBuild Build = new UnrealBuild(this);
			if (!Unversioned && !ProjectOnly)
			{
				Logger.LogInformation("Updating Version files to CL: {CL}", CL);
				Build.UpdateVersionFiles(ActuallyUpdateVersionFiles: true, ChangelistNumberOverride: CL, IsPromotedOverride: false);
			}

			// Build everything
			if (BuildProject && ExitStatus == ExitCode.Success)
			{
				Logger.LogDebug("Building Editor for {ProjectArgForEditor}", ProjectArgForEditor);

				// Invalidate the location of the Target.cs files incase they were synced
				if (ProjectFile != null)
				{
					string TargetSourceDir = CommandUtils.CombinePaths(Path.GetDirectoryName(ProjectFile.FullName), "Source");
					Rules.InvalidateRulesFileCache(TargetSourceDir);
				}

				BuildEditor BuildCmd = new BuildEditor();
				BuildCmd.Params = this.Params;
				ExitStatus = BuildCmd.Execute();
			}
			
			// If both -build and -open are specified
			// on the commandline, then we will rely on the successful build to call
			// it's open of the editor. Otherwise just open the editor
			if (!BuildProject && OpenProject && ExitStatus == ExitCode.Success)
			{
				Logger.LogDebug("Opening Editor for {ProjectArgForEditor}", ProjectArgForEditor);

				OpenEditor OpenCmd = new OpenEditor();
				OpenCmd.Params = this.Params;
				ExitStatus = OpenCmd.Execute();
			}

		}

		return ExitCode.Success;
	}	
}
