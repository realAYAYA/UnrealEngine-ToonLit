// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Serialization;
using Microsoft.Extensions.Logging;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	/// <summary>
	/// Highly experimental, a work in progress and not currently in active use.
	/// * This executor uses Horde Compute to build actions remotely. Requests are made to Horde via a local Zen server.
	/// * Currently only supported for Linux and Mac toolchains.
	/// 
	/// Current Requirements:
	/// * -PreprocessDepends: Run a prepass of clang to generate the dependency list. Needed to get all input files required to compile.
	///						  This will add additional overhead to the build and is not recommended unless using Horde compute remote distribution.
	/// * -NoPCH:			  Cannot build with pch, as absolute paths are embedded into the .gch file for included headers, and these paths must exist exactly.
	/// * -HordeCompute:	  Flag to enable this executor
	/// * ZenServer:		  Must be running with the correct credentials to connect to both Horde Storage to upload files and Horde to make execution requests.
	///						  Can be launched via -LaunchZenServer.
	///
	/// TODO:
	/// * Investigate overhead when connecting from Zen to Horde
	/// * Local process racing when remote tasks are blocking more tasks from starting, or at the end of a build.
	/// * Print more concise and useful summary at the end of the build
	/// * Additional toolchain platform support
	/// * Dynamic control over the number of remote processes to run at once. Needs knowledge about availibility of remote compute resources, if the pending remote queue is overloaded and by how much, etc.
	/// </summary>
	class HordeExecutor : ParallelExecutor
	{
		/// <summary>
		/// How many processes that will be executed in parallel remotely
		/// </summary>
		[XmlConfigFile]
		private static int NumRemoteParallelProcesses = 24;

		/// <summary>
		/// Only run work remotely, for testing
		/// </summary>
		[XmlConfigFile]
		private static bool RemoteProcessOnly = false;

		/// <summary>
		/// Retry actions locally when failed remotely.
		/// </summary>
		[XmlConfigFile]
		private static bool RetryFailedRemote = true;

		/// <summary>
		/// Temporary debug flag to allow launching zenserver.
		/// </summary>
		private static bool LaunchZenServer = Environment.CommandLine.ToLower().Contains("-launchzenserver");

		private static readonly Lazy<HttpClient> LazyZenHttpClient = new Lazy<HttpClient>();
		private static HttpClient ZenHttpClient { get { return LazyZenHttpClient.Value; } }

		// Manager to prevent rehashing and reuploading duplicate data to Zen.
		private static readonly Lazy<ZenUploader> LazyHashLoader = new Lazy<ZenUploader>();
		private static ZenUploader HashLoader { get { return LazyHashLoader.Value; } }

		internal class ZenWorker
		{
			internal class ZenFile : IComparable<ZenFile>
			{
				[CbField("name")]
				public string? Name { get; set; }
				[CbField("hash")]
				public CbBinaryAttachment? Hash { get; set; }
				[CbField("size")]
				public long? Size { get; set; }

				public int CompareTo(ZenFile? Other)
				{
					return string.Compare(Name, Other?.Name);
				}
			}

			[CbField("path")]
			public string? Path { get; set; }
			[CbField("workdir")]
			public string? WorkingDirectory { get; set; }
			[CbField("arguments")]
			public List<string>? Arguments { get; set; } = new List<string>();
			[CbField("executables")]
			public List<ZenFile>? Executables { get; set; } = new List<ZenFile>();
			[CbField("files")]
			public List<ZenFile>? Files { get; set; } = new List<ZenFile>();
			[CbField("directories")]
			public List<string>? Directories { get; set; } = new List<string>();
			[CbField("environment")]
			public List<string>? Environment { get; set; } = new List<string>();
			[CbField("outputs")]
			public List<string>? Outputs { get; set; } = new List<string>();
			[CbField("host")]
			public string? HostPlatform { get; set; }
			[CbField("cores")]
			public int? Cores { get; set; }
			[CbField("memory")]
			public long? Memory { get; set; }
			[CbField("exclusive")]
			public bool? Excusive { get; set; }
			[CbField("timeout")]
			public int? TimeoutSeconds { get; set; }
		}

		internal class ZenWorkerPostResult
		{
			[CbField("need")]
			public List<IoHash>? Need { get; set; } = new List<IoHash>();
		}

		internal class ZenWorkerResult
		{
			internal class ZenWorkerResultFile
			{
				[CbField("name")]
				public string? Name { get; set; }
				[CbField("data")]
				public byte[]? Data { get; set; }
			}

			[DebuggerDisplay("{Name}: {Time}")]
			internal class ZenWorkerResultStat : IComparable<ZenWorkerResultStat>
			{
				[CbField("name")]
				public string? Name { get; set; }
				[CbField("time")]
				public DateTime? Time { get; set; }

				public int CompareTo(ZenWorkerResultStat? Other)
				{
					return DateTime.Compare(Time ?? DateTime.MaxValue, Other?.Time ?? DateTime.MaxValue);
				}
			}

			[CbField("agent")]
			public string? Agent { get; set; }
			[CbField("detail")]
			public string? Detail { get; set; }
			[CbField("stdout")]
			public string? StdOut { get; set; }
			[CbField("stderr")]
			public string? StdErr { get; set; }
			[CbField("exitcode")]
			public int? ExitCode { get; set; }
			[CbField("files")]
			public List<ZenWorkerResultFile>? Files { get; set; } = new List<ZenWorkerResultFile>();

			[CbField("stats")]
			public List<ZenWorkerResultStat>? Stats { get; set; } = new List<ZenWorkerResultStat>();
		}

		internal class ZenUploader
		{
			Dictionary<string, IoHash> FileHashes = new Dictionary<string, IoHash>();

			Dictionary<IoHash, byte[]> Datas = new Dictionary<IoHash, byte[]>();
			HashSet<IoHash> Uploaded = new HashSet<IoHash>();

			public IoHash HashFile(FileReference File)
			{
				if (!FileHashes.ContainsKey(File.FullName))
				{
					byte[] Data = FileReference.ReadAllBytes(File);
					IoHash Hash = HashAndStoreData(Data);
					lock (FileHashes)
					{
						FileHashes.TryAdd(File.FullName, Hash);
					}
				}
				return FileHashes[File.FullName];
			}

			// HashString is not cached
			public Tuple<IoHash, long> HashString(string String)
			{
				byte[] Data = Encoding.ASCII.GetBytes(String);
				return Tuple.Create(HashAndStoreData(Data), Data.LongLength);
			}

			private IoHash HashAndStoreData(byte[] Data)
			{
				IoHash Hash = IoHash.Compute(Data);
				lock (Datas)
				{
					Datas.TryAdd(Hash, Data);
				}
				return Hash;
			}

			public bool GetData(IoHash Hash, [System.Diagnostics.CodeAnalysis.NotNullWhen(true)] out ReadOnlyMemory<byte> Memory)
			{
				Memory = null;
				if (Datas.TryGetValue(Hash, out byte[]? Data))
				{
					Memory = Data;
					return true;
				}
				return false;
			}

			public async Task UploadData(IoHash Hash, CancellationToken CancellationToken)
			{
				if (Uploaded.Contains(Hash))
				{
					return;
				}
				if (!GetData(Hash, out ReadOnlyMemory<byte> Data))
				{
					throw new BuildException($"Missing data {Hash}");
				}
				var Uri = $"http://localhost:1337/cas/{Hash}";
				var HttpPutResponse = await ZenHttpClient.PutAsync(Uri, new ReadOnlyMemoryContent(Data), CancellationToken);
				HttpPutResponse.EnsureSuccessStatusCode();
				lock (Uploaded)
				{
					Uploaded.Add(Hash);
				}
				lock (Datas)
				{
					Datas.Remove(Hash);
				}
			}

			public void MarkAsUploaded(IoHash Hash)
			{
				if (Uploaded.Contains(Hash))
				{
					return;
				}
				lock (Uploaded)
				{
					Uploaded.Add(Hash);
				}
				lock (Datas)
				{
					Datas.Remove(Hash);
				}
			}

			public void MarkAsUploaded(IEnumerable<IoHash> Hashes)
			{
				foreach (IoHash Hash in Hashes)
				{
					MarkAsUploaded(Hash);
				}
			}
		}

		protected class RemoteExecuteResults : ExecuteResults
		{
			public Dictionary<string, DateTime> Timepoints { get; private set; }

			public RemoteExecuteResults(List<string> LogLines, int ExitCode, TimeSpan ExecutionTime, TimeSpan ProcessorTime, Dictionary<string, DateTime> Timepoints) : base(LogLines, ExitCode, ExecutionTime, ProcessorTime)
			{
				this.Timepoints = Timepoints;
				AdditionalDescription = "Remote";
			}

			public RemoteExecuteResults(List<string> LogLines, int ExitCode) : base(LogLines, ExitCode)
			{
				Timepoints = new Dictionary<string, DateTime>();
				AdditionalDescription = "Remote";
			}
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="MaxLocalActions">How many local actions to execute in parallel</param>
		/// <param name="bAllCores">Consider logical cores when determining how many total cpu cores are available</param>
		/// <param name="bCompactOutput"></param>
		/// <param name="Logger"></param>
		public HordeExecutor(int MaxLocalActions, bool bAllCores, bool bCompactOutput, ILogger Logger) : base(MaxLocalActions, bAllCores, bCompactOutput, Logger)
		{
			XmlConfig.ApplyTo(this);
		}

		/// <summary>
		/// Returns the name of this executor
		/// </summary>
		public override string Name
		{
			get { return "Horde"; }
		}

		/// <summary>
		/// Checks whether the task executor can be used
		/// </summary>
		/// <returns>True if the task executor can be used</returns>
		public static new bool IsAvailable()
		{
			try
			{
				if (LaunchZenServer)
				{
					ConfigHierarchy EngineConfig = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.Combine(Unreal.EngineDirectory, "Programs", "UnrealBuildTool"), BuildHostPlatform.Current.Platform);

					if (EngineConfig.GetBool("Zen", "AutoLaunch", out bool AutoLaunch) && AutoLaunch)
					{
						return true;
					}
				}

				return IsReady();
			}
			catch
			{
				return false;
			}
		}

		private static bool IsReady()
		{
			try
			{
				var HttpPostResult = ZenHttpClient.GetAsync("http://localhost:1337/apply/ready");
				HttpPostResult.Wait();
				return HttpPostResult.Result.IsSuccessStatusCode;
			}
			catch
			{
				return false;
			}
		}

		private static bool LaunchZen(ManagedProcessGroup ProcessGroup, ILogger Logger, [System.Diagnostics.CodeAnalysis.NotNullWhen(true)] out ManagedProcess? ZenProcess)
		{
			ZenProcess = null;
			try
			{
				ConfigHierarchy EngineConfig = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.Combine(Unreal.EngineDirectory, "Programs", "UnrealBuildTool"), BuildHostPlatform.Current.Platform);

				if (!EngineConfig.GetBool("Zen", "AutoLaunch", out bool AutoLaunch) || !AutoLaunch)
				{
					Logger.LogWarning("Unable to launch zenserver: '[Zen] AutoLaunch' not enabled in Engine config");
					return false;
				}

				if (!EngineConfig.GetString("Zen.AutoLaunch", "DataPath", out string DataPath) || string.IsNullOrEmpty(DataPath))
				{
					Logger.LogWarning("Unable to launch zenserver: '[Zen.AutoLaunch] DataPath' not set in Engine config");
					return false;
				}

				if (!EngineConfig.GetString("Zen.AutoLaunch", "ExtraArgs", out string ExtraArgs) || string.IsNullOrEmpty(ExtraArgs))
				{
					Logger.LogWarning("Unable to launch zenserver: '[Zen.AutoLaunch] ExtraArgs' not set in Engine config");
					return false;
				}

				if (DataPath.Contains("%APPSETTINGSDIR%"))
				{
					DirectoryReference? CommonPath = null;
					if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win64)
					{
						CommonPath = DirectoryReference.FromString(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData), "Epic"));
					}
					else if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
					{
						CommonPath = DirectoryReference.FromString(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), "Library", "Application Support", "Epic"));
					}
					else if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Linux || BuildHostPlatform.Current.Platform == UnrealTargetPlatform.LinuxArm64)
					{
						CommonPath = DirectoryReference.FromString(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), ".config", "Epic"));
					}

					if (CommonPath == null)
					{
						Logger.LogWarning("Unable to launch zenserver: data-dir root directory not found");
						return false;
					}

					DataPath = DataPath.Replace("%APPSETTINGSDIR%", $"{CommonPath.FullName}{Path.DirectorySeparatorChar}");
				}

				DirectoryReference? DataPathReference = DirectoryReference.FromString(DataPath);
				if (DataPathReference == null || DataPathReference.ParentDirectory == null)
				{
					Logger.LogWarning("Unable to launch zenserver: data-dir not found");
					return false;
				}

				// Copy Zen to DataPath
				FileReference ZenSourcePath = FileReference.Combine(Unreal.EngineDirectory, "Binaries", BuildHostPlatform.Current.Platform.ToString(), $"zenserver{(RuntimeInformation.IsOSPlatform(OSPlatform.Windows) ? ".exe" : "")}");
				FileReference ZenDestPath = FileReference.Combine(DataPathReference.ParentDirectory, "Install", ZenSourcePath.GetFileName());

				Logger.LogInformation("Copying zenserver to '{ZenDestPath}'", ZenDestPath);
				DirectoryReference.CreateDirectory(DataPathReference);
				DirectoryReference.CreateDirectory(ZenDestPath.Directory);
				FileReference.MakeWriteable(ZenDestPath);
				FileReference.Copy(ZenSourcePath, ZenDestPath, true);

				Logger.LogInformation("Launching zenserver with data-dir '{DataPathReference}'...", DataPathReference);
				ZenProcess = new ManagedProcess(ProcessGroup, ZenDestPath.FullName, $"--port 1337 --data-dir \"{DataPathReference.FullName}\" {ExtraArgs}", ZenDestPath.Directory.FullName, null, ProcessPriorityClass.Normal);

				return true;
			}
			catch (Exception Ex)
			{
				Log.WriteException(Ex, null);
				return false;
			}
		}

		/// <summary>
		/// Executes the specified actions locally.
		/// </summary>
		/// <returns>True if all the tasks successfully executed, or false if any of them failed.</returns>
		public override bool ExecuteActions(List<LinkedAction> InputActions, ILogger Logger)
		{
			int NumCompletedActions = 0;
			int TotalActions = InputActions.Count;
			int ActualNumParallelProcesses = Math.Min(TotalActions, NumParallelProcesses);

			using ManagedProcessGroup ProcessGroup = new ManagedProcessGroup();
			ManagedProcess? ZenProcess = null;
			if (!IsReady())
			{
				const double TotalWaitTime = 10.0;
				DateTime WaitStartTime = DateTime.Now;
				if (LaunchZenServer && LaunchZen(ProcessGroup, Logger, out ZenProcess))
				{
					while ((DateTime.Now - WaitStartTime).TotalSeconds < TotalWaitTime)
					{
						Task.Delay(100).Wait();
						if (IsReady())
						{
							break;
						}
					}
				}
				if (!IsReady())
				{
					if (LaunchZenServer)
					{
						Logger.LogError("Unable to establish connection to zenserver after {TotalWaitTime}s", TotalWaitTime);
						return false;
					}
					Log.TraceInformationOnce("Unable to establish connection to zenserver, disabling HordeExecutor");
					return base.ExecuteActions(InputActions, Logger);
				}
				else
				{
					Logger.LogInformation("Established connection to zenserver in {Time}s", (DateTime.Now - WaitStartTime).TotalSeconds);
				}
			}

			using SemaphoreSlim MaxProcessSemaphore = new SemaphoreSlim(ActualNumParallelProcesses, ActualNumParallelProcesses);
			using SemaphoreSlim MaxRemoteProcessSemaphore = new SemaphoreSlim(NumRemoteParallelProcesses, NumRemoteParallelProcesses);
			using ProgressWriter ProgressWriter = new ProgressWriter("Compiling C++ source code...", false, Logger);

			Logger.LogInformation("Building {NumActions} {Actions} with {NumLocalProcesses} local {LocalProcesses} and {NumRemoteProcesses} remote {RemoteProcesses}...", TotalActions, (TotalActions == 1) ? "action" : "actions", ActualNumParallelProcesses, (ActualNumParallelProcesses == 1) ? "process" : "processes", NumRemoteParallelProcesses, (NumRemoteParallelProcesses == 1) ? "process" : "processes");

			if (RemoteProcessOnly)
			{
				Logger.LogWarning("Processing all distributable actions remotely. This is not recommended unless testing remote compute.");
			}

			Dictionary<LinkedAction, Task<ExecuteResults>> ExecuteTasks = new Dictionary<LinkedAction, Task<ExecuteResults>>();
			List<Task> LogTasks = new List<Task>();

			using LogIndentScope Indent = new LogIndentScope("  ");

			CancellationTokenSource CancellationTokenSource = new CancellationTokenSource();
			CancellationToken CancellationToken = CancellationTokenSource.Token;

			// Create a task for every action
			foreach (LinkedAction Action in InputActions)
			{
				if (ExecuteTasks.ContainsKey(Action))
				{
					continue;
				}

				Task<ExecuteResults> ExecuteTask = CreateRemoteExecuteTask(Action, ExecuteTasks, ProcessGroup, MaxProcessSemaphore, MaxRemoteProcessSemaphore, CancellationToken, Logger);
				Task LogTask = ExecuteTask.ContinueWith(antecedent => LogCompletedAction(Action, antecedent, CancellationTokenSource, ProgressWriter, TotalActions, ref NumCompletedActions, Logger), CancellationToken);

				ExecuteTasks.Add(Action, ExecuteTask);
				LogTasks.Add(LogTask);
			}

			Task.Factory.ContinueWhenAll(LogTasks.ToArray(), (AntecedentTasks) => TraceSummary(ExecuteTasks, ProcessGroup, Logger), CancellationToken)
				.ContinueWith(antecedent => TraceRemoteSummary(ExecuteTasks, Logger))
				.Wait();

			// Return if all tasks succeeded
			return ExecuteTasks.Values.All(x => x.Result.ExitCode == 0);
		}

		private static void TraceStats(List<RemoteExecuteResults> RemoteExecuteResults, string Description, string StartTime, string EndTime, ILogger Logger)
		{
			var Results = RemoteExecuteResults.Where(x => x.Timepoints.ContainsKey(StartTime) && x.Timepoints.ContainsKey(EndTime)).ToArray();
			if (Results.Length == 0)
			{
				return;
			}

			Logger.LogInformation("  {Description}:", Description);
			Logger.LogInformation("    Total {Total:0.00}", Results.Sum(x => (x.Timepoints[EndTime] - x.Timepoints[StartTime]).TotalSeconds));
			Logger.LogInformation("    Avg   {Average:0.00}", Results.Average(x => (x.Timepoints[EndTime] - x.Timepoints[StartTime]).TotalSeconds));
			Logger.LogInformation("    Min   {Min:0.00}", Results.Min(x => (x.Timepoints[EndTime] - x.Timepoints[StartTime]).TotalSeconds));
			Logger.LogInformation("    Max   {Max:0.00}", Results.Max(x => (x.Timepoints[EndTime] - x.Timepoints[StartTime]).TotalSeconds));
		}

		private static void TraceRemoteSummary(Dictionary<LinkedAction, Task<ExecuteResults>> Tasks, ILogger Logger)
		{
			// TODO: Beautify summary
			List<ExecuteResults> LocalExecuteResults = Tasks.Values.Where(x => x.IsCompletedSuccessfully && x.Result.ExitCode == 0 && !(x.Result is RemoteExecuteResults)).Select(x => x.Result).ToList();
			List<RemoteExecuteResults> RemoteExecuteResults = Tasks.Values.Where(x => x.IsCompletedSuccessfully && x.Result.ExitCode == 0 && x.Result is RemoteExecuteResults).Select(x => (RemoteExecuteResults)x.Result).ToList();
			Logger.LogInformation("");
			Logger.LogInformation("Successful Tasks Run: Local {LocalCount} Remote {RemoteCount}", LocalExecuteResults.Count, RemoteExecuteResults.Count);
			TraceStats(RemoteExecuteResults, "ubt iohash", "ubt-queue-dispatched", "ubt-hash-files", Logger);
			TraceStats(RemoteExecuteResults, "ubt put files", "ubt-hash-files", "ubt-put-files", Logger);
			TraceStats(RemoteExecuteResults, "zen queue wait", "zen-queue-added", "zen-queue-dispatched", Logger);
			TraceStats(RemoteExecuteResults, "zen put horde", "zen-storage-build-ref", "zen-storage-put-ref", Logger);
			TraceStats(RemoteExecuteResults, "horde queue wait", "horde-queue-added", "horde-queue-dispatched", Logger);
			TraceStats(RemoteExecuteResults, "horde prepare sandbox", "horde-execution-start", "horde-execution-download-input", Logger);
			TraceStats(RemoteExecuteResults, "horde execute", "horde-execution-download-input", "horde-execution-execute", Logger);
			TraceStats(RemoteExecuteResults, "horde upload results", "horde-execution-execute", "horde-execution-upload-ref", Logger);
			TraceStats(RemoteExecuteResults, "zen queue results wait", "zen-complete-queue-added", "zen-complete-queue-dispatched", Logger);
			TraceStats(RemoteExecuteResults, "zen get horde", "zen-complete-queue-dispatched", "zen-storage-get-blobs", Logger);
			TraceStats(RemoteExecuteResults, "zen finalize", "zen-storage-get-blobs", "zen-queue-complete", Logger);
			Logger.LogInformation("");
		}

		private static Task<ExecuteResults> CreateRemoteExecuteTask(LinkedAction Action, Dictionary<LinkedAction, Task<ExecuteResults>> ExecuteTasks, ManagedProcessGroup ProcessGroup, SemaphoreSlim MaxProcessSemaphore, SemaphoreSlim MaxRemoteProcessSemaphore, CancellationToken CancellationToken, ILogger Logger)
		{
			Task<ExecuteResults> ActionTask;
			if (Action.PrerequisiteActions.Count == 0)
			{
				ActionTask = Task.Factory.StartNew(
					() => ExecuteRemoteAction(Array.Empty<Task<ExecuteResults>>(), Action, ProcessGroup, MaxProcessSemaphore, MaxRemoteProcessSemaphore, CancellationToken),
					CancellationToken,
					TaskCreationOptions.LongRunning | TaskCreationOptions.PreferFairness,
					TaskScheduler.Current
				).Unwrap();
			}
			else
			{

				// Create tasks for any preresquite actions if they don't exist already
				List<Task<ExecuteResults>> PrerequisiteTasks = new List<Task<ExecuteResults>>();
				foreach (var PrerequisiteAction in Action.PrerequisiteActions)
				{
					if (!ExecuteTasks.ContainsKey(PrerequisiteAction))
					{
						ExecuteTasks.Add(PrerequisiteAction, CreateRemoteExecuteTask(PrerequisiteAction, ExecuteTasks, ProcessGroup, MaxProcessSemaphore, MaxRemoteProcessSemaphore, CancellationToken, Logger));
					}
					PrerequisiteTasks.Add(ExecuteTasks[PrerequisiteAction]);
				}

				ActionTask = Task.Factory.ContinueWhenAll(
					PrerequisiteTasks.ToArray(),
					(AntecedentTasks) => ExecuteRemoteAction(AntecedentTasks, Action, ProcessGroup, MaxProcessSemaphore, MaxRemoteProcessSemaphore, CancellationToken),
					CancellationToken,
					TaskContinuationOptions.LongRunning | TaskContinuationOptions.PreferFairness,
					TaskScheduler.Current
				).Unwrap();
			}

			if (RetryFailedRemote)
			{
				return ActionTask.ContinueWith(ancendent =>
				{
					if (ancendent.IsCanceled || ancendent.Result.ExitCode == 0 || ancendent.Result is not RemoteExecuteResults)
					{
						return Task.FromResult(ancendent.Result);
					}
					Logger.LogInformation("Remote execute for {CommandDescription} {StatusDescription} failed, rescheduling for local execution.", (Action.CommandDescription ?? Action.CommandPath.GetFileNameWithoutExtension()), Action.StatusDescription);
					return ExecuteAction(Array.Empty<Task<ExecuteResults>>(), Action, ProcessGroup, MaxProcessSemaphore, CancellationToken);
				},
				CancellationToken,
				TaskContinuationOptions.LongRunning | TaskContinuationOptions.PreferFairness,
				TaskScheduler.Current).Unwrap();
			}

			return ActionTask;
		}

		private static HashSet<FileReference>? GatherDependencies(FileItem DependencyListFile, IEnumerable<FileItem> PrerequisiteItems)
		{
			DependencyListFile.ResetCachedInfo();
			// TODO: Share with CppDependencyCache in BuildMode.Build and remove TryGetDependenciesUncached
			CppDependencyCache.TryGetDependenciesUncached(DependencyListFile, out List<FileItem>? DependencyFileItems);
			if (DependencyFileItems == null || DependencyFileItems.Count == 0)
			{
				return null;
			}

			HashSet<FileReference> DependencyItems = DependencyFileItems.Distinct().Select(x => new FileReference(x.FullName)).ToHashSet();
			DependencyItems.UnionWith(PrerequisiteItems.Where(x => x != DependencyListFile).Select(x => new FileReference(x.FullName)));
			return DependencyItems;
		}

		private static async Task<ExecuteResults> ExecuteRemoteAction(Task<ExecuteResults>[] AntecedentTasks, LinkedAction Action, ManagedProcessGroup ProcessGroup, SemaphoreSlim MaxProcessSemaphore, SemaphoreSlim MaxRemoteProcessSemaphore, CancellationToken CancellationToken)
		{
			Task<bool>? SemaphoreTask = null;
			Task<bool>? RemoteSemaphoreTask = null;
			try
			{
				// Cancel tasks if any PrerequisiteActions fail, unless a PostBuildStep
				if (Action.ActionType != ActionType.PostBuildStep && AntecedentTasks.Any(x => x.Result.ExitCode != 0))
				{
					throw new OperationCanceledException();
				}

				// Run locally if available, skipping any dependency checking or file hashing
				if (!RemoteProcessOnly)
				{
					SemaphoreTask = MaxProcessSemaphore.WaitAsync(0, CancellationToken);
					await SemaphoreTask;

					if (SemaphoreTask.Status == TaskStatus.RanToCompletion && SemaphoreTask.Result)
					{
						return await RunAction(Action, ProcessGroup, CancellationToken);
					}
					CancellationToken.ThrowIfCancellationRequested();
				}

				// Gather dependencies from Dependency file if can execute remotely
				HashSet<FileReference>? DependencyItems = null;
				if (Action.bCanExecuteRemotely && Action.DependencyListFile != null && Action.PrerequisiteItems.Any(x => x == Action.DependencyListFile))
				{
					DependencyItems = GatherDependencies(Action.DependencyListFile!, Action.PrerequisiteItems);
				}

				// Unable to gather dependencies, must build local
				if (DependencyItems == null || DependencyItems.Count == 0)
				{
					// Run ParallelExecutor.ExecuteAction
					return await ExecuteAction(AntecedentTasks, Action, ProcessGroup, MaxProcessSemaphore, CancellationToken);
				}

				while (true)
				{
					if (!RemoteProcessOnly)
					{
						SemaphoreTask = MaxProcessSemaphore.WaitAsync(100, CancellationToken);
						await SemaphoreTask;

						if (SemaphoreTask.Result)
						{
							break;
						}
					}

					RemoteSemaphoreTask = MaxRemoteProcessSemaphore.WaitAsync(0, CancellationToken);
					await RemoteSemaphoreTask;

					if (RemoteSemaphoreTask.Result)
					{
						break;
					}
					CancellationToken.ThrowIfCancellationRequested();
				}

				if (SemaphoreTask != null && SemaphoreTask.Status == TaskStatus.RanToCompletion && SemaphoreTask.Result)
				{
					// Local Execution
					return await RunAction(Action, ProcessGroup, CancellationToken);
				}
				else if (RemoteSemaphoreTask != null && RemoteSemaphoreTask.Status == TaskStatus.RanToCompletion && RemoteSemaphoreTask.Result)
				{
					// Remote Execution
					return await RunRemoteAction(Action, DependencyItems, CancellationToken);
				}
				else
				{
					// Unexpected
					throw new OperationCanceledException();
				}
			}
			catch (OperationCanceledException)
			{
				return new ExecuteResults(new List<string>(), int.MaxValue);
			}
			catch (Exception Ex)
			{
				if (RemoteSemaphoreTask?.Status == TaskStatus.RanToCompletion && RemoteSemaphoreTask?.Result == true)
				{
					if (!RetryFailedRemote)
					{
						Log.WriteException(Ex, null);
					}
					return new RemoteExecuteResults(new List<string>(), int.MaxValue);
				}
				Log.WriteException(Ex, null);
				return new ExecuteResults(new List<string>(), int.MaxValue);
			}
			finally
			{
				if (SemaphoreTask?.Status == TaskStatus.RanToCompletion && SemaphoreTask?.Result == true)
				{
					MaxProcessSemaphore.Release();
				}

				if (RemoteSemaphoreTask?.Status == TaskStatus.RanToCompletion && RemoteSemaphoreTask?.Result == true)
				{
					MaxRemoteProcessSemaphore.Release();
				}
			}
		}

		private static async Task<RemoteExecuteResults> RunRemoteAction(LinkedAction Action, HashSet<FileReference> DependencyItems, CancellationToken CancellationToken)
		{
			Dictionary<string, DateTime> Timepoints = new Dictionary<string, DateTime>();
			Timepoints["ubt-queue-dispatched"] = DateTime.UtcNow;

			ZenWorker? Worker = new ZenWorker
			{
				Path = Action.CommandPath.IsUnderDirectory(Unreal.RootDirectory) ? Action.CommandPath.MakeRelativeTo(Unreal.EngineSourceDirectory) : Action.CommandPath.FullName,
				WorkingDirectory = Unreal.EngineSourceDirectory.MakeRelativeTo(Unreal.RootDirectory),
				Arguments = new List<string>() { Action.CommandArguments.Trim() },
				Directories = new List<string>() { Unreal.EngineSourceDirectory.MakeRelativeTo(Unreal.RootDirectory) },
				HostPlatform = BuildHostPlatform.Current.Platform.ToString(),
				Cores = 1,
				Memory = MemoryPerActionBytesOverride > 0 ? (long)MemoryPerActionBytesOverride : 1 * 1024 * 1024 * 1024, // 1 GB default
				TimeoutSeconds = 300,
				Executables = new List<ZenWorker.ZenFile>(),
				Files = new List<ZenWorker.ZenFile>(),
				Outputs = new List<string>(Action.ProducedItems.Distinct().Select(x => FileReference.FromString(x.FullName).MakeRelativeTo(Unreal.RootDirectory)))
			};

			foreach (var SourceFile in DependencyItems!.Where(SourceFile => SourceFile.IsUnderDirectory(Unreal.RootDirectory)))
			{
				Worker.Files.Add(new ZenWorker.ZenFile { Name = SourceFile.MakeRelativeTo(Unreal.RootDirectory), Hash = HashLoader.HashFile(SourceFile), Size = SourceFile.ToFileInfo().Length });
			}
			Timepoints["ubt-hash-files"] = DateTime.UtcNow;

			// TODO: Running command through a script file shouldn't be necessary
			if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win64)
			{
				StringBuilder ScriptContents = new StringBuilder();
				ScriptContents.AppendLine("@echo off");
				ScriptContents.AppendLine($"\"{Action.CommandPath.FullName}\" {Action.CommandArguments.Trim()}");

				var Result = HashLoader.HashString(ScriptContents.ToString());
				Worker.Executables.Add(new ZenWorker.ZenFile { Name = Path.Combine(Worker.WorkingDirectory, "Run.bat"), Hash = Result.Item1, Size = Result.Item2 });

				Worker.Path = "cmd.exe";
				Worker.Arguments.Clear();
				Worker.Arguments.Add("/C");
				Worker.Arguments.Add("Run.bat");
			}
			else if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac || BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Linux)
			{
				StringBuilder ScriptContents = new StringBuilder();
				ScriptContents.AppendLine("#!/bin/sh");
				ScriptContents.AppendLine($"'{Action.CommandPath}' {Action.CommandArguments.Trim()}");

				var Result = HashLoader.HashString(ScriptContents.ToString());
				Worker.Executables.Add(new ZenWorker.ZenFile { Name = Path.Combine(Worker.WorkingDirectory, "Run.sh"), Hash = Result.Item1, Size = Result.Item2 });

				Worker.Path = "/bin/sh";
				Worker.Arguments.Clear();
				Worker.Arguments.Add("Run.sh");
			}

			Worker.Executables?.Sort();
			Worker.Files?.Sort();
			Worker.Directories?.Sort();
			Worker.Environment?.Sort();
			Worker.Outputs?.Sort();

			IoHash WorkerHash;
			{
				CbObject Object = CbSerializer.Serialize(Worker);
				WorkerHash = Object.GetHash();

				ReadOnlyMemoryContent Content = new ReadOnlyMemoryContent(Object.GetView());
				Content.Headers.Add("Content-Type", "application/x-ue-cb");
				var HttpPostResult = await ZenHttpClient.PostAsync($"http://localhost:1337/apply/workers/{WorkerHash}", Content, CancellationToken);
				if (HttpPostResult.StatusCode == System.Net.HttpStatusCode.NotFound)
				{
					CancellationToken.ThrowIfCancellationRequested();
					ZenWorkerPostResult NeedResult = CbSerializer.Deserialize<ZenWorkerPostResult>(await HttpPostResult.Content.ReadAsByteArrayAsync(CancellationToken));
					if (NeedResult.Need?.Count > 0)
					{
						foreach (var Need in NeedResult.Need)
						{
							await HashLoader.UploadData(Need, CancellationToken);
						}

						Content = new ReadOnlyMemoryContent(Object.GetView());
						Content.Headers.Add("Content-Type", "application/x-ue-cb");
						HttpPostResult = await ZenHttpClient.PostAsync($"http://localhost:1337/apply/workers/{WorkerHash}", Content, CancellationToken);
					}
				}
				HttpPostResult.EnsureSuccessStatusCode();
				CancellationToken.ThrowIfCancellationRequested();
				HashLoader.MarkAsUploaded(Worker.Executables!.Where(x => x.Hash.HasValue).Select(x => x.Hash!.Value.Hash));
				HashLoader.MarkAsUploaded(Worker.Files!.Where(x => x.Hash.HasValue).Select(x => x.Hash!.Value.Hash));
			}
			Timepoints["ubt-put-files"] = DateTime.UtcNow;
			Worker = null;

			{
				using var Request = new HttpRequestMessage(HttpMethod.Post, $"http://localhost:1337/apply/simple/{WorkerHash}");
				Request.Headers.Add("Accept", "application/x-ue-cb");
				var HttpPostResult = await ZenHttpClient.SendAsync(Request, CancellationToken);
				HttpPostResult.EnsureSuccessStatusCode();
				CancellationToken.ThrowIfCancellationRequested();
			}

			byte[]? Buffer = null;
			{
				DateTime ResultsStartTime = DateTime.Now;
				while ((DateTime.UtcNow - Timepoints["ubt-queue-dispatched"]).TotalSeconds < 600)
				{
					using var Request = new HttpRequestMessage(HttpMethod.Get, $"http://localhost:1337/apply/simple/{WorkerHash}");
					Request.Headers.Add("Accept", "application/x-ue-cb");
					var HttpGetResult = await ZenHttpClient.SendAsync(Request, CancellationToken);
					HttpGetResult.EnsureSuccessStatusCode();

					if (HttpGetResult.StatusCode == System.Net.HttpStatusCode.Accepted)
					{
						await Task.Delay(100, CancellationToken);
						continue;
					}

					// TODO: Streaming?
					Buffer = await HttpGetResult.Content.ReadAsByteArrayAsync(CancellationToken);
					CancellationToken.ThrowIfCancellationRequested();
					break;
				}
			}

			if (Buffer == null)
			{
				throw new BuildException("Failed remote execution");
			}

			ZenWorkerResult OutputData = CbSerializer.Deserialize<ZenWorkerResult>(Buffer);
			List<string> LogLines = new List<string>();
			if (!string.IsNullOrEmpty(OutputData.StdOut))
			{
				LogLines.AddRange(OutputData.StdOut.Split('\n'));
			}
			if (!string.IsNullOrEmpty(OutputData.StdErr))
			{
				LogLines.AddRange(OutputData.StdErr.Split('\n'));
			}
			int ExitCode = OutputData.ExitCode ?? 0;

			if (ExitCode == 0)
			{
				foreach (var ResultFile in OutputData.Files!)
				{
					FileReference File = FileReference.Combine(Unreal.RootDirectory, ResultFile.Name!);
					await System.IO.File.WriteAllBytesAsync(File.FullName, ResultFile.Data!, CancellationToken);
					CancellationToken.ThrowIfCancellationRequested();
				}
			}

			OutputData.Stats?.Sort();
			OutputData.Stats?.ForEach(x => { if (x.Name != null && x.Time.HasValue) Timepoints[x.Name] = x.Time.Value; });
			Timepoints["ubt-queue-complete"] = DateTime.UtcNow;

			return new RemoteExecuteResults(LogLines, ExitCode, Timepoints["ubt-queue-complete"] - Timepoints["ubt-queue-dispatched"], TimeSpan.Zero, Timepoints);
		}
	}
}
