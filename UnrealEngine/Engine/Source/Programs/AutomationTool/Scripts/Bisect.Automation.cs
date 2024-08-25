// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using Microsoft.Extensions.Logging;
using System.Linq;
using UnrealBuildTool;
using UnrealBuildBase;

namespace AutomationTool
{
	[Help("Bisects a range of changelists between 'good' and 'bad' (exclusive). If a failed-build is reported, then bisection expands outwards from the active changelist until bisection resumed.")]
	[Help("TargetStream=<Stream>", "Stream path. (Required)")]
	[Help("RootPath=<Stream>", "Local root path (if not specified current client root path will be used (not recommended as some files used by the script might be replaced while syncing)). (Optional)")]
	[Help("GoodCL=<CL>", "First changelist that is in good shape. (Required)")]
	[Help("BadCL=<CL>", "Changelist that is known to be bad. (Required)")]
	[Help("Project=<Project.uproject>", "Project file name to be synced. (Required)")]
	[Help("TestsToCheck", "Test list to check the tests should be separated by semicolon. (Optional)")]
	[RequireP4]

	class Bisect : BuildCommand
	{
		/// <summary>
		/// The class is a helper class for bisection method.
		/// <para>
		/// It stores a range of changelist indexes (between 'good' and 'bad' (exclusive)) to support bisection of the current range by calling <c>Bad</c> and <c>Good</c> methods.
		/// The class also supports <c>Ugly</c> method to label current changelist index with 'failed to build' mark.
		/// </para>
		/// </summary>
		private class Bisectomatron
		{
			public int GoodIndex { get; private set; }
			public int BadIndex { get; private set; }
			public int Index { get; private set; }
			private int Jump;
			private HashSet<int> UglyIndexes;

			/// <summary>
			/// The class constructor
			/// </summary>
			/// <param name="Length">Length of the desired range.</param>
			public Bisectomatron(int Length)
			{
				GoodIndex = 0;
				BadIndex = Length - 1;
				Jump = -1;
				UglyIndexes = new HashSet<int>();
				Bisect();
			}

			/// <summary>
			/// The method is designed to check whether the object of the class is still valid to proceed bisection.
			/// </summary>
			/// <param name="BisectomatronInstance">The object to check whether it is valid.</param>
			public static implicit operator bool(Bisectomatron BisectomatronInstance)
			{
				return (BisectomatronInstance.BadIndex - BisectomatronInstance.GoodIndex) > (BisectomatronInstance.UglyIndexes.Count + 1);
			}

			/// <summary>
			/// The method performs bisection of the current indexes range with taking into account indexes of changelists that were not built correctly.
			/// </summary>
			private void Bisect()
			{
				Index = (GoodIndex + BadIndex) / 2;
				while (UglyIndexes.Contains(Index))
				{
					Index += Jump;
				}
			}

			/// <summary>
			/// The method handles current index as 'good' (build and test runs for the corresponding changelist were completed successfully) and proceed bisection if possible.
			/// </summary>
			public void Good()
			{
				UglyIndexes.RemoveWhere(UglyIndex => (UglyIndex <= Index));
				GoodIndex = Index;
				Jump = 1;
				Bisect();
			}

			/// <summary>
			/// The method handles current index as 'bad' (test run for the corresponding changelist was completed with error) and proceed bisection if possible.
			/// </summary>
			public void Bad()
			{
				UglyIndexes.RemoveWhere(UglyIndex => (UglyIndex >= Index));
				BadIndex = Index;
				Jump = -1;
				Bisect();
			}

			/// <summary>
			/// The method handles current index as 'ugly' (build of corresponding changelist was completed with error) and tries to determine the closest neighboring index that has not been checked yet.
			/// <para>
			/// While searching for the closest neighboring index that has not been checked yet the method goes in two directions.
			/// It is performed by alternating the closest left hand side and right hand side neighbors that have not been handled yet.
			/// </para>
			/// </summary>
			public void Ugly()
			{
				UglyIndexes.Add(Index);

				// try to set the nearest neighbor index that is not in UglyIndexes.
				while (this)
				{
					int CurrentJump = Jump;
					Jump = -CurrentJump - (CurrentJump / Math.Abs(CurrentJump));
					Index += CurrentJump;
					if (!UglyIndexes.Contains(Index))
					{
						break;
					}
				}
			}
		}

		/// <summary>
		/// The class is a storage class for the project file information.
		/// </summary>
		private class ProjectFileInfo
		{
			public string DepotPath { get; set; }
			public string FilePath { get; set; }

			/// <summary>
			/// Checks if the stored data is valid.
			/// </summary>
			/// <returns>True if the corresponding object stores valid data, otherwise - false.</returns>
			public bool IsValid()
			{
				return
					(!String.IsNullOrEmpty(DepotPath)) &&
					(!String.IsNullOrEmpty(FilePath));
			}

			/// <summary>
			/// The method returns depot path for the corresponding project file.
			/// </summary>
			/// <returns>Depot path for the corresponding project file or null if it can not be determined.</returns>
			public string GetDepotDirPath()
			{
				if (String.IsNullOrEmpty(DepotPath))
				{
					return null;
				}

				string[] DepotPathParts = DepotPath.Split("/");
				string[] DepotDirPathParts = DepotPathParts.Take(DepotPathParts.Length - 1).ToArray();
				return String.Join("/", DepotDirPathParts);
			}
		}

		private static readonly List<string> RequiredDirsToSync = new List<string>{ "Engine" };
		private static readonly List<string> DirsToExcludeFromSync = new List<string> { 
			"Engine/Binaries/ThirdParty/DotNet",
			"Engine/Source/Programs/AutomationTool"
		};
		
		private string TargetStream { get; set; }
		private string RootPath { get; set; }
		private string Project { get; set; }
		private int GoodCL { get; set; }
		private int BadCL { get; set; }
		private string TestsToCheck { get; set; }

		private List<string> GetRequiredDirsToSync()
		{
			List<string> Result = new List<string>();
			
			foreach (string Dir in RequiredDirsToSync)
			{
				Result.Add(String.Format("{0}/{1}", TargetStream, Dir));
			}
			
			return Result;
		}

		private List<KeyValuePair<string, string>> GetExclusionViewParts()
		{
			List<KeyValuePair<string, string>> Result = new List<KeyValuePair<string, string>>();

			// Apply exclusion list if and only if RootPath is not temporary (e.g it is local root)
			if (RootPath == CommandUtils.CmdEnv.LocalRoot)
			{
				foreach (string Dir in DirsToExcludeFromSync)
				{
					Result.Add(new KeyValuePair<string, string>(
						String.Format("-{0}/{1}/...", TargetStream, Dir),
						String.Format("/{0}/...", Dir)));
				}
			}

			return Result;
		}

		private void GetEntriesToSync(P4Connection Perforce, int CL, ProjectFileInfo ProjectFileInfo, out List<string> Dirs, out List<string> Files)
		{
			Dirs = GetRequiredDirsToSync();
			string ProjectDirDepotPath = ProjectFileInfo.GetDepotDirPath();
			if (!String.IsNullOrEmpty(ProjectDirDepotPath))
			{
				string[] ProjectDirDepotPathParts = ProjectDirDepotPath.Split("/");
				if (!Dirs.Any(Dir => {
					string[] DirPathParts = Dir.Split("/");
					int PartsToCompareCount = Math.Min(DirPathParts.Length, ProjectDirDepotPathParts.Length);
					bool bDirsHasTheSameDepotPathStart = Enumerable.SequenceEqual(ProjectDirDepotPathParts.Take(PartsToCompareCount), DirPathParts.Take(PartsToCompareCount));
					return bDirsHasTheSameDepotPathStart;
				}))
				{
					Dirs.Add(ProjectDirDepotPath);
				}
			}

			string FStatOutput;
			// Note that the command line containt '*' symbol. Because of that all the additional parameters after fstat will be placed into a temorary file.
			// Every parameter in this temporary file should be in a separate line (using of '\n' instead of ' ').
			string FStatCommandLine = String.Format("fstat -Olhp\n-Dl\n-F\n^headAction=delete & ^headAction=move/delete\n{0}/*@{1}", TargetStream, CL);
			if (!Perforce.LogP4Output(out FStatOutput, "", FStatCommandLine))
			{
				throw new AutomationException(String.Format("p4 {0} failed", FStatCommandLine));
			}

			Files = new List<string>();
			FStatOutput = FStatOutput.Replace("\r", "");
			string[] OutputLines = FStatOutput.Split("\n");
			string FilePrefix = new string("... depotFile ");

			foreach (string OutputLine in OutputLines)
			{
				string TrimmedOutputLine = OutputLine.Trim();
				if (TrimmedOutputLine.StartsWith(FilePrefix))
				{
					Files.Add(TrimmedOutputLine.Substring(FilePrefix.Length));
				}
			}
		}

		private ProjectFileInfo GetProjectFileInfo(P4Connection Perforce)
		{
			if (String.IsNullOrEmpty(TargetStream) || String.IsNullOrEmpty(Project))
			{
				return null;
			}

			string FStatOutput;
			string FStatCommandLine = String.Format("fstat -Olhp -Dl -F \"^headAction=delete & ^headAction=move/delete\" {0}/.../{1}", TargetStream, Project);
			Perforce.LogP4Output(out FStatOutput, "", FStatCommandLine);

			FStatOutput = FStatOutput.Replace("\r", "");
			string[] OutputLines = FStatOutput.Split("\n");
			string DepotFilePathPrefix = new string("... depotFile ");
			string FilePathPrefix = new string("... path ");

			ProjectFileInfo Result = new ProjectFileInfo();
			foreach (string OutputLine in OutputLines)
			{
				string TrimmedOutputLine = OutputLine.Trim();
				if (TrimmedOutputLine.EndsWith(Project))
				{
					if (TrimmedOutputLine.StartsWith(DepotFilePathPrefix))
					{
						Result.DepotPath = TrimmedOutputLine.Substring(DepotFilePathPrefix.Length);
					}
					else if (TrimmedOutputLine.StartsWith(FilePathPrefix))
					{
						Result.FilePath = TrimmedOutputLine.Substring(FilePathPrefix.Length);
					}

					if (Result.IsValid())
					{
						return Result;
					}
				}
			}

			return null;
		}
		private bool RunExecutableInRootEnvironment(string ExecutablePath, string CommandLineArgs)
		{
			string DirectoryToRestore = Environment.CurrentDirectory;
			if (!CommandUtils.ChDir_NoExceptions(RootPath))
			{
				return false;
			}

			Dictionary<string, string> OverridenEnv = new Dictionary<string, string>();
			OverridenEnv.Add("uebp_FinalLogFolder", CommandUtils.CombinePaths(RootPath, "Engine/Programs/AutomationTool/Saved/Logs"));
			OverridenEnv.Add("uebp_EngineSavedFolder", CommandUtils.CombinePaths(RootPath, "Engine/Programs/AutomationTool/Saved"));
			OverridenEnv.Add("uebp_LOCAL_ROOT", RootPath);
			OverridenEnv.Add("uebp_LogFolder", CommandUtils.CombinePaths(RootPath, "Engine/Programs/AutomationTool/Saved/Logs"));

			IProcessResult ProcessResult;
			try
			{
				ProcessResult = CommandUtils.Run(ExecutablePath, CommandLineArgs, null, ERunOptions.Default, OverridenEnv, null, "Bisect.BuildProject", RootPath);
			}
			catch
			{
				return false;
			}
			finally
			{
				CommandUtils.ChDir_NoExceptions(DirectoryToRestore);
			}

			return ProcessResult.ExitCode == 0;
		}

		private bool BuildProject(ProjectFileInfo ProjectFileInfo)
		{
			string CommandLineArgs = String.Format("BuildCookRun -project={0} -platform={1} -build", ProjectFileInfo.FilePath, HostPlatform.Current.HostEditorPlatform.ToString());
			bool bResult = true;

			if (RootPath == CommandUtils.CmdEnv.LocalRoot)
			{
				try
				{
					CommandUtils.RunUAT(CommandUtils.CmdEnv, CommandLineArgs, "BisectBuild");
				}
				catch
				{
					bResult =  false;
				}
			}
			else
			{
				string ExecutablePath = CommandUtils.CombinePaths(RootPath, "RunUAT");
				if (HostPlatform.Current.HostEditorPlatform == UnrealTargetPlatform.Win64)
				{
					ExecutablePath += ".bat";
				}
				else
				{
					ExecutablePath += ".sh";
				}

				bResult = RunExecutableInRootEnvironment(ExecutablePath, CommandLineArgs);
			}

			return bResult;
		}

		private bool RunTests()
		{
			string ExecutablePath = CommandUtils.CombinePaths(RootPath, "Engine/Binaries", HostPlatform.Current.HostEditorPlatform.ToString(), "UnrealEditor");
			if (HostPlatform.Current.HostEditorPlatform == UnrealTargetPlatform.Win64)
			{
				ExecutablePath += ".exe";
			}

			string CommandLineArgs = String.Format(" {0} -ExecCmds=\"Automation RunTests {1};Quit;\"", Project, TestsToCheck);

			return RunExecutableInRootEnvironment(ExecutablePath,CommandLineArgs);
		}

		private void ParseCommandLineArgs()
		{
			TargetStream = ParseParamValue("TargetStream");
			if (TargetStream == null)
			{
				throw new AutomationException("TargetStream is not specified");
			}
			TargetStream = TargetStream.TrimEnd(new char[] { '/', '\\' });
			if (TargetStream.Length == 0)
			{
				throw new AutomationException("TargetStream is empty");
			}

			RootPath = ParseParamValue("RootPath");
			if (RootPath == null)
			{
				RootPath = CommandUtils.CmdEnv.LocalRoot;
			}
			else
			{
				RootPath = RootPath.TrimEnd(new char[] { '/', '\\' });
				if (RootPath.Length == 0)
				{
					RootPath = CommandUtils.CmdEnv.LocalRoot;
				}
			}
			
			Project = ParseParamValue("Project");
			if (String.IsNullOrEmpty(Project))
			{
				throw new AutomationException("Project is not specified");
			}

			GoodCL = ParseParamInt("GoodCL");
			BadCL = ParseParamInt("BadCL");
			if (GoodCL >= BadCL)
			{
				throw new AutomationException("Invalid range of changelists given (GoodCL >= BadCL)");
			}

			TestsToCheck = ParseParamValue("TestsToCheck");
		}

		/// <summary>
		/// Executes the command
		/// </summary>
		public override void ExecuteBuild()
		{
			ParseCommandLineArgs();

			string PerforceClientName = String.Format("{0}_{1}_Automation_Bisect_Temp", P4Env.User, Unreal.MachineName);
			bool bPerforceClientCreatedOutside = P4.DoesClientExist(PerforceClientName);

			if (!bPerforceClientCreatedOutside)
			{
				List<KeyValuePair<string, string>> RequiredView = new List<KeyValuePair<string, string>>();
				RequiredView.Add(new KeyValuePair<string, string>(
					String.Format("{0}/...", TargetStream),
					String.Format("/...", PerforceClientName)));
				RequiredView.AddRange(GetExclusionViewParts());

				P4ClientInfo PerforceClientInfo = new P4ClientInfo();
				PerforceClientInfo.Owner = P4Env.User;
				PerforceClientInfo.Host = Unreal.MachineName;
				PerforceClientInfo.RootPath = RootPath;
				PerforceClientInfo.Name = PerforceClientName;
				PerforceClientInfo.View = RequiredView;
				PerforceClientInfo.Stream = null;
				PerforceClientInfo.Options = P4ClientOption.NoAllWrite | P4ClientOption.Clobber | P4ClientOption.NoCompress | P4ClientOption.Unlocked | P4ClientOption.NoModTime | P4ClientOption.RmDir;
				PerforceClientInfo.LineEnd = P4LineEnd.Local;
				P4.CreateClient(PerforceClientInfo);
			}

			// Sync the workspace and delete the client
			try
			{
				P4Connection Perforce = new P4Connection(P4Env.User, PerforceClientName);

				// Build p4 command line. '-L' enables changelist descriptions, '-l' enables full descriptions, '-s submitted' ignores shelves and pending changes
				string ChangesCommandLine = String.Format("-L -l -s submitted {0}/...@{1},{2}", TargetStream, GoodCL, BadCL);

				Perforce.Changes(out List<P4Connection.ChangeRecord> SubmittedCandidateChanges, ChangesCommandLine);

				if (SubmittedCandidateChanges.Count < 3)
				{
					throw new AutomationException("Perforce returned too few changelists between {0} and {1}", GoodCL, BadCL);
				}

				Bisectomatron IndexGenerator = new Bisectomatron(SubmittedCandidateChanges.Count);

				while (IndexGenerator)
				{
					int Index = IndexGenerator.Index;
					P4Connection.ChangeRecord ChangeRecord = SubmittedCandidateChanges[Index];
					int WorkingCL = ChangeRecord.CL;

					ProjectFileInfo ProjectFileInfo = GetProjectFileInfo(Perforce);
					if ((ProjectFileInfo == null) || (!ProjectFileInfo.IsValid()))
					{
						throw new AutomationException("Can not determine project info for project name \"{0}\" and CL {1}", Project, WorkingCL);
					}

					List<string> DirsToSync;
					List<string> FilesToSync;
					GetEntriesToSync(Perforce, WorkingCL, ProjectFileInfo, out DirsToSync, out FilesToSync);

					string SyncCommandLine = new string("");
					if (DirsToSync.Any())
					{
						Logger.LogInformation("Dirs to sync:");
						foreach (string DirToSync in DirsToSync)
						{
							SyncCommandLine += String.Format(" {0}/...@{1}", DirToSync, WorkingCL);
							Logger.LogInformation(" {0}", DirToSync);
						}
					}

					if (FilesToSync.Any())
					{
						Logger.LogInformation("Files to sync:");
						foreach (string FileToSync in FilesToSync)
						{
							SyncCommandLine += String.Format(" {0}@{1}", FileToSync, WorkingCL);
							Logger.LogInformation(" {0}", FileToSync);
						}
					}
					if (String.IsNullOrEmpty(SyncCommandLine))
					{
						// There is nothing to sync
						throw new AutomationException("There is nothing to sync");
					}

					Logger.LogInformation("Syncing to {SyncCL}...", WorkingCL);
					Perforce.Sync(SyncCommandLine);
					Logger.LogInformation("Syncing to {SyncCL} has been finished.", WorkingCL);

					if (!BuildProject(ProjectFileInfo))
					{
						IndexGenerator.Ugly();
						continue;
					}

					if (!String.IsNullOrEmpty(TestsToCheck))
					{
						if (RunTests())
						{
							IndexGenerator.Good();
						}
						else
						{
							IndexGenerator.Bad();
						}
					}
				}

				Logger.LogInformation("Bisection complete.");
				Logger.LogInformation("Good change: {0}", SubmittedCandidateChanges[IndexGenerator.GoodIndex].CL);
				Logger.LogInformation("Bad change: {0}", SubmittedCandidateChanges[IndexGenerator.BadIndex].CL);
			}
			catch
			{
				throw;
			}
			finally
			{
				if (!bPerforceClientCreatedOutside)
				{
					P4.DeleteClient(PerforceClientName);
				}
			}
		}
	}
}
