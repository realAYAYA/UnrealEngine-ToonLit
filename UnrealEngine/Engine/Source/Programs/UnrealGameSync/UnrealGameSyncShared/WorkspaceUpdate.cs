// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Text.Encodings.Web;
using System.Text.Json;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Perforce;
using Microsoft.Extensions.Logging;

namespace UnrealGameSync
{
	[Flags]
	public enum WorkspaceUpdateOptions
	{
		Sync = 0x01,
		SyncSingleChange = 0x02,
		AutoResolveChanges = 0x04,
		GenerateProjectFiles = 0x08,
		SyncArchives = 0x10,
		Build = 0x20,
		Clean = 0x40,
		ScheduledBuild = 0x80,
		RunAfterSync = 0x100,
		OpenSolutionAfterSync = 0x200,
		ContentOnly = 0x400,
		UpdateFilter = 0x800,
		SyncAllProjects = 0x1000,
		IncludeAllProjectsInSolution = 0x2000,
		RemoveFilteredFiles = 0x4000,
		Clobber = 0x8000,
		Refilter = 0x10000,
	}

	public enum WorkspaceUpdateResult
	{
		Canceled,
		FailedToSync,
		FailedToSyncLoginExpired,
		FilesToDelete,
		FilesToResolve,
		FilesToClobber,
		FailedToCompile,
		FailedToCompileWithCleanWorkspace,
		Success,
	}

	public class PerforceSyncOptions
	{
		public const int DefaultNumRetries = 0;
		public const int DefaultNumThreads = 4;
		public const int DefaultTcpBufferSize = 0;
		public const int DefaultFileBufferSize = 0;

		public const int DefaultMaxCommandsPerBatch = 200;
		public const int DefaultMaxSizePerBatch = 128 * 1024 * 1024;
		public const int DefaultNumSyncErrorRetries = 0;

		public int? NumThreads { get; set; }

		public int? MaxCommandsPerBatch { get; set; }
		public int? MaxSizePerBatch { get; set; }

		public int? NumSyncErrorRetries { get; set; }

		public PerforceSyncOptions Clone()
		{
			return (PerforceSyncOptions)MemberwiseClone();
		}
	}

	public class WorkspaceUpdateContext
	{
		public DateTime StartTime { get; set; } = DateTime.UtcNow;
		public int ChangeNumber { get; set; }
		public int? CodeChangeNumber { get; set; }
		public WorkspaceUpdateOptions Options { get; set; }
		public BuildConfig EditorConfig { get; set; }
		public List<string> SyncFilter { get; } = new List<string>();
		public Dictionary<string, IArchive?> ArchiveTypeToArchive { get; } = new Dictionary<string, IArchive?>();
		public Dictionary<string, bool> DeleteFiles { get; } = new Dictionary<string, bool>();
		public Dictionary<string, bool> ClobberFiles { get; } = new Dictionary<string, bool>();
		public List<ConfigObject> UserBuildStepObjects { get; } = new List<ConfigObject>();
		public HashSet<Guid> CustomBuildSteps { get; } = new HashSet<Guid>();
		public Dictionary<string, string> AdditionalVariables { get; } = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
		public PerforceSyncOptions? PerforceSyncOptions { get; set; }
		public List<HaveRecord> HaveFiles { get; } = new List<HaveRecord>(); // Cached when sync filter has changed

		// May be updated during sync
		public ConfigFile? ProjectConfigFile { get; set; }
		public IReadOnlyList<string>? ProjectStreamFilter { get; set; }

		public WorkspaceUpdateContext(int changeNumber, WorkspaceUpdateOptions options, BuildConfig editorConfig, string[]? syncFilter, List<ConfigObject> userBuildSteps, HashSet<Guid>? customBuildSteps)
		{
			ChangeNumber = changeNumber;
			Options = options;
			EditorConfig = editorConfig;
			if (syncFilter != null)
			{
				SyncFilter.AddRange(syncFilter);
			}
			UserBuildStepObjects = userBuildSteps;
			if (customBuildSteps != null)
			{
				CustomBuildSteps.UnionWith(customBuildSteps);
			}
		}

		public static WorkspaceUpdateOptions GetOptionsFromConfig(GlobalSettings globalSettings, UserWorkspaceSettings workspaceSettings)
		{
			WorkspaceUpdateOptions options = 0;
			if (globalSettings.AutoResolveConflicts)
			{
				options |= WorkspaceUpdateOptions.AutoResolveChanges;
			}
			if (workspaceSettings.Filter.AllProjects ?? globalSettings.Filter.AllProjects ?? false)
			{
				options |= WorkspaceUpdateOptions.SyncAllProjects | WorkspaceUpdateOptions.IncludeAllProjectsInSolution;
			}
			if (workspaceSettings.Filter.AllProjectsInSln ?? globalSettings.Filter.AllProjectsInSln ?? false)
			{
				options |= WorkspaceUpdateOptions.IncludeAllProjectsInSolution;
			}
			return options;
		}
	}

	public class WorkspaceSyncCategory
	{
		public Guid UniqueId { get; set; }
		public bool Enable { get; set; }
		public string Name { get; set; }
		public List<string> Paths { get; } = new List<string>();
		public bool Hidden { get; set; }
		public List<Guid> Requires { get; } = new List<Guid>();

		public WorkspaceSyncCategory(Guid uniqueId) : this(uniqueId, "Unnamed")
		{
		}

		public WorkspaceSyncCategory(Guid uniqueId, string name, params string[] paths)
		{
			UniqueId = uniqueId;
			Enable = true;
			Name = name;
			Paths.AddRange(paths);
		}

		public static Dictionary<Guid, bool> GetDefault(IEnumerable<WorkspaceSyncCategory> categories)
		{
			return categories.ToDictionary(x => x.UniqueId, x => x.Enable);
		}

		public static Dictionary<Guid, bool> GetDelta(Dictionary<Guid, bool> source, Dictionary<Guid, bool> target)
		{
			Dictionary<Guid, bool> changes = new Dictionary<Guid, bool>();
			foreach (KeyValuePair<Guid, bool> pair in target)
			{
				bool value;
				if (!source.TryGetValue(pair.Key, out value) || value != pair.Value)
				{
					changes[pair.Key] = pair.Value;
				}
			}
			return changes;
		}

		public static void ApplyDelta(Dictionary<Guid, bool> categories, Dictionary<Guid, bool> delta)
		{
			foreach (KeyValuePair<Guid, bool> pair in delta)
			{
				categories[pair.Key] = pair.Value;
			}
		}

		public override string ToString()
		{
			return Name;
		}
	}

	public class ProjectInfo
	{
		public DirectoryReference LocalRootPath { get; } // ie. local mapping of clientname + branchpath

		public string ClientName { get; }
		public string BranchPath { get; } // starts with a slash if non-empty. does not end with a slash.
		public string ProjectPath { get; } // starts with a slash, uses forward slashes

		public string? StreamName { get; } // name of the current stream

		public string ProjectIdentifier { get; } // stream path to project
		public bool IsEnterpriseProject { get; } // whether it's an enterprise project

		// derived properties
		public FileReference LocalFileName => new FileReference(LocalRootPath.FullName + ProjectPath);
		public string ClientRootPath => $"//{ClientName}{BranchPath}";
		public string ClientFileName => $"//{ClientName}{BranchPath}{ProjectPath}";
		public string TelemetryProjectIdentifier => PerforceUtils.GetClientOrDepotDirectoryName(ProjectIdentifier);
		public DirectoryReference EngineDir => DirectoryReference.Combine(LocalRootPath, "Engine");
		public DirectoryReference? ProjectDir => ProjectPath.EndsWith(".uproject", StringComparison.OrdinalIgnoreCase) ? LocalFileName.Directory : null;
		public DirectoryReference DataFolder => GetDataFolder(LocalRootPath);
		public DirectoryReference CacheFolder => GetCacheFolder(LocalRootPath);

		public static DirectoryReference GetDataFolder(DirectoryReference workspaceDir) => DirectoryReference.Combine(workspaceDir, ".ugs");
		public static DirectoryReference GetCacheFolder(DirectoryReference workspaceDir) => DirectoryReference.Combine(workspaceDir, ".ugs", "cache");

		public ProjectInfo(DirectoryReference localRootPath, ReadOnlyWorkspaceState state)
			: this(localRootPath, state.ClientName, state.BranchPath, state.ProjectPath, state.StreamName, state.ProjectIdentifier, state.IsEnterpriseProject)
		{
		}

		public ProjectInfo(DirectoryReference localRootPath, string clientName, string branchPath, string projectPath, string? streamName, string projectIdentifier, bool isEnterpriseProject)
		{
			ValidateBranchPath(branchPath);
			ValidateProjectPath(projectPath);

			LocalRootPath = localRootPath;
			ClientName = clientName;
			BranchPath = branchPath;
			ProjectPath = projectPath;
			StreamName = streamName;
			ProjectIdentifier = projectIdentifier;
			IsEnterpriseProject = isEnterpriseProject;
		}

		public static async Task<ProjectInfo> CreateAsync(IPerforceConnection perforceClient, UserWorkspaceSettings settings, CancellationToken cancellationToken)
		{
			string? streamName = await perforceClient.GetCurrentStreamAsync(cancellationToken);

			// Get a unique name for the project that's selected. For regular branches, this can be the depot path. For streams, we want to include the stream name to encode imports.
			string newSelectedProjectIdentifier;
			if (streamName != null)
			{
				string expectedPrefix = String.Format("//{0}/", perforceClient.Settings.ClientName);
				if (!settings.ClientProjectPath.StartsWith(expectedPrefix, StringComparison.InvariantCultureIgnoreCase))
				{
					throw new UserErrorException($"Unexpected client path; expected '{settings.ClientProjectPath}' to begin with '{expectedPrefix}'");
				}
				string? streamPrefix = await TryGetStreamPrefixAsync(perforceClient, streamName, cancellationToken);
				if (streamPrefix == null)
				{
					throw new UserErrorException("Unable to get stream prefix");
				}
				newSelectedProjectIdentifier = String.Format("{0}/{1}", streamPrefix, settings.ClientProjectPath.Substring(expectedPrefix.Length));
			}
			else
			{
				List<PerforceResponse<WhereRecord>> records = await perforceClient.TryWhereAsync(settings.ClientProjectPath, cancellationToken).Where(x => !x.Succeeded || !x.Data.Unmap).ToListAsync(cancellationToken);
				if (!records.Succeeded() || records.Count != 1)
				{
					throw new UserErrorException($"Couldn't get depot path for {settings.ClientProjectPath}");
				}

				newSelectedProjectIdentifier = records[0].Data.DepotFile;

				Match match = Regex.Match(newSelectedProjectIdentifier, "//([^/]+)/");
				if (match.Success)
				{
					DepotRecord depot = await perforceClient.GetDepotAsync(match.Groups[1].Value, cancellationToken);
					if (depot.Type == "stream")
					{
						throw new UserErrorException($"Cannot use a legacy client ({perforceClient.Settings.ClientName}) with a stream depot ({depot.Depot}).");
					}
				}
			}

			// Figure out if it's an enterprise project. Use the synced version if we have it.
			bool isEnterpriseProject = false;
			if (settings.ClientProjectPath.EndsWith(".uproject", StringComparison.InvariantCultureIgnoreCase))
			{
				string text;
				if (FileReference.Exists(settings.LocalProjectPath))
				{
					text = await FileReference.ReadAllTextAsync(settings.LocalProjectPath, cancellationToken);
				}
				else
				{
					PerforceResponse<PrintRecord<string[]>> projectLines = await perforceClient.TryPrintLinesAsync(settings.ClientProjectPath, cancellationToken);
					if (!projectLines.Succeeded)
					{
						throw new UserErrorException($"Unable to get contents of {settings.ClientProjectPath}");
					}
					text = String.Join("\n", projectLines.Data.Contents!);
				}
				isEnterpriseProject = Utility.IsEnterpriseProjectFromText(text);
			}

			return new ProjectInfo(settings.RootDir, settings.ClientName, settings.BranchPath, settings.ProjectPath, streamName, newSelectedProjectIdentifier, isEnterpriseProject);
		}

		static async Task<string?> TryGetStreamPrefixAsync(IPerforceConnection perforce, string streamName, CancellationToken cancellationToken)
		{
			string? currentStreamName = streamName;
			while (!String.IsNullOrEmpty(currentStreamName))
			{
				PerforceResponse<StreamRecord> response = await perforce.TryGetStreamAsync(currentStreamName, false, cancellationToken);
				if (!response.Succeeded)
				{
					return null;
				}

				StreamRecord streamSpec = response.Data;
				if (streamSpec.Type != "virtual")
				{
					return currentStreamName;
				}

				currentStreamName = streamSpec.Parent;
			}
			return null;
		}

		public static void ValidateBranchPath(string branchPath)
		{
			if (branchPath.Length > 0 && (!branchPath.StartsWith("/", StringComparison.Ordinal) || branchPath.EndsWith("/", StringComparison.Ordinal)))
			{
				throw new ArgumentException("Branch path must start with a slash, and not end with a slash", nameof(branchPath));
			}
		}

		public static void ValidateProjectPath(string projectPath)
		{
			if (!projectPath.StartsWith("/", StringComparison.Ordinal))
			{
				throw new ArgumentException("Project path must start with a slash", nameof(projectPath));
			}
			if (!projectPath.EndsWith(".uproject", StringComparison.OrdinalIgnoreCase) && !projectPath.EndsWith(".uprojectdirs", StringComparison.OrdinalIgnoreCase))
			{
				throw new ArgumentException("Project path must be to a .uproject or .uprojectdirs file", nameof(projectPath));
			}
		}
	}

	public class WorkspaceUpdate
	{
		const string BuildVersionFileName = "/Engine/Build/Build.version";
		const string VersionHeaderFileName = "/Engine/Source/Runtime/Launch/Resources/Version.h";
		const string ObjectVersionFileName = "/Engine/Source/Runtime/Core/Private/UObject/ObjectVersion.cpp";

		static readonly string LocalVersionHeaderFileName = VersionHeaderFileName.Replace('/', Path.DirectorySeparatorChar);
		static readonly string LocalObjectVersionFileName = ObjectVersionFileName.Replace('/', Path.DirectorySeparatorChar);

		static readonly SemaphoreSlim _updateSemaphore = new SemaphoreSlim(1);

		class RecordCounter : IDisposable
		{
			readonly ProgressValue _progress;
			readonly string _message;
			int _count;
			readonly Stopwatch _timer = Stopwatch.StartNew();

			public RecordCounter(ProgressValue progress, string message)
			{
				_progress = progress;
				_message = message;

				progress.Set(message);
			}

			public void Dispose()
			{
				UpdateMessage();
			}

			public void Increment()
			{
				_count++;
				if (_timer.ElapsedMilliseconds > 250)
				{
					UpdateMessage();
				}
			}

			public void UpdateMessage()
			{
				_progress.Set(String.Format("{0} ({1:N0})", _message, _count));
				_timer.Restart();
			}
		}

		class SyncBatchBuilder
		{
			public int MaxCommandsPerList { get; }
			public long MaxSizePerList { get; }
			public Queue<List<string>> Batches { get; }

			List<string>? _commands;
			List<string>? _deleteCommands;
			long _size;

			public SyncBatchBuilder(int? maxCommandsPerList, long? maxSizePerList)
			{
				MaxCommandsPerList = maxCommandsPerList ?? PerforceSyncOptions.DefaultMaxCommandsPerBatch;
				MaxSizePerList = maxSizePerList ?? PerforceSyncOptions.DefaultMaxSizePerBatch;
				Batches = new Queue<List<string>>();
			}

			public void Add(string newCommand, long newSize)
			{
				if (newSize == 0)
				{
					if (_deleteCommands == null || _deleteCommands.Count >= MaxCommandsPerList)
					{
						_deleteCommands = new List<string>();
						Batches.Enqueue(_deleteCommands);
					}

					_deleteCommands.Add(newCommand);
				}
				else
				{
					if (_commands == null || _commands.Count >= MaxCommandsPerList || _size + newSize >= MaxSizePerList)
					{
						_commands = new List<string>();
						Batches.Enqueue(_commands);
						_size = 0;
					}

					_commands.Add(newCommand);
					_size += newSize;
				}
			}
		}

		class SyncTree
		{
			public bool CanUseWildcard;
			public int TotalIncludedFiles;
			public long TotalSize;
			public int TotalExcludedFiles;
			public Dictionary<string, long> IncludedFiles = new Dictionary<string, long>();
			public Dictionary<string, SyncTree> NameToSubTree = new Dictionary<string, SyncTree>(StringComparer.OrdinalIgnoreCase);

			public SyncTree(bool canUseWildcard)
			{
				CanUseWildcard = canUseWildcard;
			}

			public SyncTree FindOrAddSubTree(string name)
			{
				SyncTree? result;
				if (!NameToSubTree.TryGetValue(name, out result))
				{
					result = new SyncTree(CanUseWildcard);
					NameToSubTree.Add(name, result);
				}
				return result;
			}

			public void IncludeFile(string path, long size, ILogger logger) => IncludeFile(path, path, size, logger);

			private void IncludeFile(string fullPath, string path, long size, ILogger logger)
			{
				int idx = path.IndexOf('/', StringComparison.Ordinal);
				if (idx == -1)
				{
					if (!IncludedFiles.ContainsKey(path))
					{
						IncludedFiles.Add(path, size);
					}
				}
				else
				{
					SyncTree subTree = FindOrAddSubTree(path.Substring(0, idx));
					subTree.IncludeFile(fullPath, path.Substring(idx + 1), size, logger);
				}
				TotalIncludedFiles++;
				TotalSize += size;
			}

			public void ExcludeFile(string path)
			{
				int idx = path.IndexOf('/', StringComparison.Ordinal);
				if (idx != -1)
				{
					SyncTree subTree = FindOrAddSubTree(path.Substring(0, idx));
					subTree.ExcludeFile(path.Substring(idx + 1));
				}
				TotalExcludedFiles++;
			}

			public void GetOptimizedSyncCommands(string prefix, int changeNumber, SyncBatchBuilder builder)
			{
				if (CanUseWildcard && TotalExcludedFiles == 0 && TotalSize < builder.MaxSizePerList)
				{
					builder.Add(String.Format("{0}/...@{1}", prefix, changeNumber), TotalSize);
				}
				else
				{
					foreach (KeyValuePair<string, long> file in IncludedFiles)
					{
						builder.Add(String.Format("{0}/{1}@{2}", prefix, file.Key, changeNumber), file.Value);
					}
					foreach (KeyValuePair<string, SyncTree> pair in NameToSubTree)
					{
						pair.Value.GetOptimizedSyncCommands(String.Format("{0}/{1}", prefix, PerforceUtils.EscapePath(pair.Key)), changeNumber, builder);
					}
				}
			}
		}

		public WorkspaceUpdateContext Context { get; }
		public ProgressValue Progress { get; } = new ProgressValue();

		public WorkspaceUpdate(WorkspaceUpdateContext context)
		{
			Context = context;
		}

		class SyncFile
		{
			public string DepotFile;
			public string RelativePath;
			public long Size;

			public SyncFile(string depotFile, string relativePath, long size)
			{
				DepotFile = depotFile;
				RelativePath = relativePath;
				Size = size;
			}
		};

		class SemaphoreScope : IDisposable
		{
			readonly SemaphoreSlim _semaphore;
			public bool HasLock { get; private set; }

			public SemaphoreScope(SemaphoreSlim semaphore)
			{
				_semaphore = semaphore;
			}

			public bool TryAcquire()
			{
				HasLock = HasLock || _semaphore.Wait(0);
				return HasLock;
			}

			public async Task AcquireAsync(CancellationToken cancellationToken)
			{
				await _semaphore.WaitAsync(cancellationToken);
				HasLock = true;
			}

			public void Release()
			{
				if (HasLock)
				{
					_semaphore.Release();
					HasLock = false;
				}
			}

			public void Dispose() => Release();
		}

		public static string ShellScriptExt { get; } = RuntimeInformation.IsOSPlatform(OSPlatform.Windows) ? "bat" : "sh";

		public static Task<int> ExecuteShellCommandAsync(string commandLine, Action<string> processOutput, CancellationToken cancellationToken)
		{
			if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				string cmdExe = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.System), "cmd.exe");
				return Utility.ExecuteProcessAsync(cmdExe, null, $"/C \"{commandLine}\"", processOutput, cancellationToken);
			}
			else
			{
				string shellExe = "/bin/sh";
				return Utility.ExecuteProcessAsync(shellExe, null, $"{commandLine}", processOutput, cancellationToken);
			}
		}

		public async Task<(WorkspaceUpdateResult, string)> ExecuteAsync(IPerforceSettings perforceSettings, ProjectInfo project, WorkspaceStateWrapper stateMgr, ILogger logger, CancellationToken cancellationToken)
		{
			using IPerforceConnection perforce = await PerforceConnection.CreateAsync(new PerforceSettings(perforceSettings) { EnableHangMonitor = false }, logger);

			ReadOnlyWorkspaceState state = stateMgr.Current;

			List<Tuple<string, TimeSpan>> times = new List<Tuple<string, TimeSpan>>();

			int numFilesSynced = 0;
			if (Context.Options.HasFlag(WorkspaceUpdateOptions.Sync) || Context.Options.HasFlag(WorkspaceUpdateOptions.SyncSingleChange))
			{
				using (TelemetryStopwatch syncTelemetryStopwatch = new TelemetryStopwatch("Workspace_Sync", project.TelemetryProjectIdentifier))
				{
					logger.LogInformation("Syncing to {Change} on {ServerAndPort} as {UserName}...", Context.ChangeNumber, perforceSettings.ServerAndPort, perforceSettings.UserName);

					// Make sure we're logged in
					PerforceResponse<LoginRecord> loginResponse = await perforce.TryGetLoginStateAsync(cancellationToken);
					if (!loginResponse.Succeeded)
					{
						return (WorkspaceUpdateResult.FailedToSyncLoginExpired, "User is not logged in.");
					}

					// Figure out which paths to sync
					List<string> relativeSyncPaths = GetRelativeSyncPaths(project, (Context.Options & WorkspaceUpdateOptions.SyncAllProjects) != 0, Context.SyncFilter);
					List<string> syncPaths = new List<string>(relativeSyncPaths.Select(x => project.ClientRootPath + x));

					// Get the user's sync filter
					FileFilter userFilter = new FileFilter(FileFilterType.Include);
					userFilter.AddRules(Context.SyncFilter.Select(x => x.Trim()).Where(x => x.Length > 0 && !x.StartsWith(";", StringComparison.Ordinal) && !x.StartsWith("#", StringComparison.Ordinal)));

					// Check if the new sync filter matches the previous one. If not, we'll enumerate all files in the workspace and make sure there's nothing extra there.
					string? nextSyncFilterHash = null;
#pragma warning disable CA5350 // Do Not Use Weak Cryptographic Algorithms
					using (SHA1 sha = SHA1.Create())
					{
						StringBuilder combinedFilter = new StringBuilder();
						foreach (string relativeSyncPath in relativeSyncPaths)
						{
							combinedFilter.AppendFormat("{0}\n", relativeSyncPath);
						}
						if (Context.SyncFilter.Count > 0)
						{
							combinedFilter.Append("--FROM--\n");
							combinedFilter.Append(String.Join("\n", Context.SyncFilter));
						}
						nextSyncFilterHash = StringUtils.FormatHexString(sha.ComputeHash(Encoding.UTF8.GetBytes(combinedFilter.ToString())));
					}
#pragma warning restore CA5350 // Do Not Use Weak Cryptographic Algorithms

					// If the hash differs, enumerate everything in the workspace to find what needs to be removed
					if (nextSyncFilterHash != state.CurrentSyncFilterHash || (Context.Options & WorkspaceUpdateOptions.Refilter) != 0)
					{
						using (TelemetryStopwatch filterStopwatch = new TelemetryStopwatch("Workspace_Sync_FilterChanged", project.TelemetryProjectIdentifier))
						{
							logger.LogInformation("Filter has changed ({PrevHash} -> {NextHash}); finding files in workspace that need to be removed.", (String.IsNullOrEmpty(state.CurrentSyncFilterHash)) ? "None" : state.CurrentSyncFilterHash, nextSyncFilterHash);

							// Find all the files that are in this workspace
							List<HaveRecord> haveFiles = Context.HaveFiles;
							if (haveFiles.Count == 0)
							{
								using (RecordCounter haveCounter = new RecordCounter(Progress, "Sync filter changed; checking workspace..."))
								{
									await foreach (PerforceResponse<HaveRecord> record in perforce.TryHaveAsync(FileSpecList.Any, cancellationToken))
									{
										if (record.Succeeded)
										{
											haveFiles.Add(record.Data);
											haveCounter.Increment();
										}
										else
										{
											return (WorkspaceUpdateResult.FailedToSync, $"Unable to query files ({record}).");
										}
									}
								}
							}

							// Build a filter for the current sync paths
							FileFilter syncPathsFilter = new FileFilter(FileFilterType.Exclude);
							foreach (string relativeSyncPath in relativeSyncPaths)
							{
								syncPathsFilter.Include(relativeSyncPath);
							}

							// Remove all the files that are not included by the filter
							const int MaxLogFiles = 1000;
							List<string> removeDepotPaths = new List<string>();
							foreach (HaveRecord haveFile in haveFiles)
							{
								try
								{
									FileReference fullPath = new FileReference(haveFile.Path);
									if (MatchFilter(project, fullPath, syncPathsFilter) && !MatchFilter(project, fullPath, userFilter))
									{
										if (removeDepotPaths.Count <= MaxLogFiles)
										{
											logger.LogInformation("  {DepotFile}", haveFile.DepotFile);
										}
										removeDepotPaths.Add(haveFile.DepotFile);
									}
								}
								catch (PathTooLongException)
								{
									// We don't actually care about this when looking for files to remove. Perforce may think that it's synced the path, and silently failed. Just ignore it.
								}
							}
							if (removeDepotPaths.Count > MaxLogFiles)
							{
								logger.LogInformation("  ...and {NumFiles} others.", removeDepotPaths.Count - MaxLogFiles);
							}

							// Check if there are any paths outside the regular sync paths
							if (removeDepotPaths.Count > 0 && (Context.Options & WorkspaceUpdateOptions.RemoveFilteredFiles) == 0)
							{
								bool deleteListMatches = true;

								Dictionary<string, bool> prevDeleteFiles = new Dictionary<string, bool>(Context.DeleteFiles, StringComparer.OrdinalIgnoreCase);

								Context.DeleteFiles.Clear();
								foreach (string removeDepotPath in removeDepotPaths)
								{
									bool delete;
									if (!prevDeleteFiles.TryGetValue(removeDepotPath, out delete))
									{
										deleteListMatches = false;
										delete = true;
									}
									Context.DeleteFiles[removeDepotPath] = delete;
								}

								if (!deleteListMatches)
								{
									return (WorkspaceUpdateResult.FilesToDelete, $"Cancelled after finding {Context.DeleteFiles.Count} files excluded by filter");
								}

								removeDepotPaths.RemoveAll(x => !Context.DeleteFiles[x]);
							}

							// Actually delete any files that we don't want
							if (removeDepotPaths.Count > 0)
							{
								// Clear the current sync filter hash. If the sync is canceled, we'll be in an indeterminate state, and we should always clean next time round.
								state = stateMgr.Modify(x => x.CurrentSyncFilterHash = "INVALID");

								// Find all the depot paths that will be synced
								HashSet<string> remainingDepotPathsToRemove = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
								remainingDepotPathsToRemove.UnionWith(removeDepotPaths);

								// Build the list of revisions to sync
								List<string> revisionsToRemove = new List<string>();
								revisionsToRemove.AddRange(removeDepotPaths.Select(x => String.Format("{0}#0", x)));

								(WorkspaceUpdateResult, string) removeResult = await SyncFileRevisions(perforce, "Removing files...", Context, revisionsToRemove, remainingDepotPathsToRemove, Progress, logger, cancellationToken);
								if (removeResult.Item1 != WorkspaceUpdateResult.Success)
								{
									return removeResult;
								}
							}

							// Update the sync filter hash. We've removed any files we need to at this point.
							state = stateMgr.Modify(x => x.CurrentSyncFilterHash = nextSyncFilterHash);
						}
					}

					// Create a filter for all the files we don't want
					FileFilter filter = new FileFilter(userFilter);
					filter.Exclude(BuildVersionFileName);
					if (Context.Options.HasFlag(WorkspaceUpdateOptions.ContentOnly))
					{
						filter.Exclude("*.usf");
						filter.Exclude("*.ush");
					}

					// Create a tree to store the sync path
					SyncTree syncTree = new SyncTree(false);
					if (!Context.Options.HasFlag(WorkspaceUpdateOptions.SyncSingleChange))
					{
						foreach (string relativeSyncPath in relativeSyncPaths)
						{
							const string wildcardSuffix = "/...";
							if (relativeSyncPath.EndsWith(wildcardSuffix, StringComparison.Ordinal))
							{
								SyncTree leaf = syncTree;

								string[] fragments = relativeSyncPath.Split('/');
								for (int idx = 1; idx < fragments.Length - 1; idx++)
								{
									leaf = leaf.FindOrAddSubTree(fragments[idx]);
								}

								leaf.CanUseWildcard = true;
							}
						}
					}

					// Find all the server changes, and anything that's opened for edit locally. We need to sync files we have open to schedule a resolve.
					SyncBatchBuilder batchBuilder = new SyncBatchBuilder(Context.PerforceSyncOptions?.MaxCommandsPerBatch, Context.PerforceSyncOptions?.MaxSizePerBatch);
					List<string> syncDepotPaths = new List<string>();
					using (RecordCounter counter = new RecordCounter(Progress, "Filtering files..."))
					{
						// Track the total new bytes that will be required on disk when syncing. Add an extra 100MB for padding.
						long requiredFreeSpace = 100 * 1024 * 1024;

						foreach (string syncPath in syncPaths)
						{
							string syncFilter = Context.Options.HasFlag(WorkspaceUpdateOptions.Sync) ? $"{Context.ChangeNumber}" : $"={Context.ChangeNumber}";

							List<SyncFile> syncFiles = new List<SyncFile>();
							await foreach (PerforceResponse<SyncRecord> response in perforce.TrySyncAsync(SyncOptions.PreviewOnly, -1, 0, -1, -1, -1, -1, $"{syncPath}@{syncFilter}", cancellationToken))
							{
								if (!response.Succeeded)
								{
									return (WorkspaceUpdateResult.FailedToSync, $"Couldn't enumerate changes matching {syncPath}.");
								}
								if (response.Info != null)
								{
									logger.LogInformation("Note: {Note}", response.Info.Data);
									continue;
								}

								SyncRecord record = response.Data;

								// Get the full local path
								string relativePath;
								try
								{
									FileReference syncFile = new FileReference(record.Path.ToString());
									relativePath = PerforceUtils.GetClientRelativePath(project.LocalRootPath, syncFile);
								}
								catch (PathTooLongException)
								{
									logger.LogInformation("The local path for {Path} exceeds the maximum allowed by Windows. Re-sync your workspace to a directory with a shorter name, or delete the file from the server.", record.Path);
									return (WorkspaceUpdateResult.FailedToSync, "File exceeds maximum path length allowed by Windows.");
								}

								// Create the sync record
								long syncSize = (record.Action == SyncAction.Deleted) ? 0 : record.FileSize;
								syncFiles.Add(new SyncFile(record.DepotFile.ToString(), relativePath, syncSize));
								counter.Increment();
							}

							// Also sync the currently open files
							await foreach (PerforceResponse<OpenedRecord> response in perforce.TryOpenedAsync(OpenedOptions.None, FileSpecList.Any, cancellationToken))
							{
								if (!response.Succeeded)
								{
									return (WorkspaceUpdateResult.FailedToSync, $"Couldn't enumerate changes matching {syncPath}.");
								}

								OpenedRecord record = response.Data;
								if (!String.IsNullOrEmpty(record.DepotFile) && !String.IsNullOrEmpty(record.ClientFile))
								{
									if (record.Action != FileAction.Add || record.Action != FileAction.Branch || record.Action != FileAction.MoveAdd)
									{
										string relativePath = PerforceUtils.GetClientRelativePath(record.ClientFile);
										syncFiles.Add(new SyncFile(record.DepotFile, relativePath, 0));
									}
								}
							}

							// Enumerate all the files to be synced. NOTE: depotPath is escaped, whereas clientPath is not.
							foreach (SyncFile syncRecord in syncFiles)
							{
								if (filter.Matches(syncRecord.RelativePath))
								{
									syncTree.IncludeFile(PerforceUtils.EscapePath(syncRecord.RelativePath), syncRecord.Size, logger);
									syncDepotPaths.Add(syncRecord.DepotFile);
									requiredFreeSpace += syncRecord.Size;

									// If the file exists the required free space can be reduced as those bytes will be replaced.
									FileInfo localFileInfo = FileReference.Combine(project.LocalRootPath, syncRecord.RelativePath).ToFileInfo();
									if (localFileInfo.Exists)
									{
										requiredFreeSpace -= localFileInfo.Length;
									}
								}
								else
								{
									syncTree.ExcludeFile(PerforceUtils.EscapePath(syncRecord.RelativePath));
								}
							}
						}

						try
						{
							DirectoryInfo localRootInfo = project.LocalRootPath.ToDirectoryInfo();
							DriveInfo drive = new DriveInfo(localRootInfo.Root.FullName);

							if (drive.AvailableFreeSpace < requiredFreeSpace)
							{
								logger.LogInformation("Syncing requires {RequiredSpace} which exceeds the {AvailableSpace} available free space on {Drive}.", StringUtils.FormatBytesString(requiredFreeSpace), StringUtils.FormatBytesString(drive.AvailableFreeSpace), drive.Name);
								return (WorkspaceUpdateResult.FailedToSync, "Not enough available free space.");
							}
						}
						catch (SystemException)
						{
							logger.LogInformation("Unable to check available free space for {RootPath}.", project.LocalRootPath);
						}
					}
					syncTree.GetOptimizedSyncCommands(project.ClientRootPath, Context.ChangeNumber, batchBuilder);

					// Clear the current sync changelist, in case we cancel
					if (!Context.Options.HasFlag(WorkspaceUpdateOptions.SyncSingleChange))
					{
						state = stateMgr.Modify(x =>
						{
							x.CurrentChangeNumber = -1;
							x.AdditionalChangeNumbers.Clear();
						});
					}

					// Find all the depot paths that will be synced
					HashSet<string> remainingDepotPaths = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
					remainingDepotPaths.UnionWith(syncDepotPaths);

					using (TelemetryStopwatch transferStopwatch = new TelemetryStopwatch("Workspace_Sync_TransferFiles", project.TelemetryProjectIdentifier))
					{
						transferStopwatch.AddData(new { MachineName = System.Net.Dns.GetHostName(), DomainName = Environment.UserDomainName, ServerAndPort = perforce.Settings.ServerAndPort, UserName = perforce.Settings.UserName, IncludedFiles = syncTree.TotalIncludedFiles, ExcludedFiles = syncTree.TotalExcludedFiles, Size = syncTree.TotalSize, NumThreads = Context.PerforceSyncOptions?.NumThreads ?? PerforceSyncOptions.DefaultNumThreads });

						(WorkspaceUpdateResult, string) syncResult = await SyncFileRevisions(perforce, "Syncing files...", Context, batchBuilder.Batches, remainingDepotPaths, Progress, logger, cancellationToken);
						if (syncResult.Item1 != WorkspaceUpdateResult.Success)
						{
							transferStopwatch.AddData(new { SyncResult = syncResult.Item2.ToString(), CompletedFilesFiles = syncDepotPaths.Count - remainingDepotPaths.Count });
							return syncResult;
						}

						transferStopwatch.Stop("Ok");
						transferStopwatch.AddData(new { TransferRate = syncTree.TotalSize / Math.Max(transferStopwatch.Elapsed.TotalSeconds, 0.0001f) });
					}

					int versionChangeNumber = -1;
					if (Context.Options.HasFlag(WorkspaceUpdateOptions.Sync) && !Context.Options.HasFlag(WorkspaceUpdateOptions.UpdateFilter))
					{
						// Read the new config file
						Context.ProjectConfigFile = await ReadProjectConfigFile(project.LocalRootPath, project.LocalFileName, logger);
						Context.ProjectStreamFilter = await ReadProjectStreamFilter(perforce, Context.ProjectConfigFile, cancellationToken);

						// Get the branch name
						string? branchOrStreamName = await perforce.GetCurrentStreamAsync(cancellationToken);
						if (branchOrStreamName != null)
						{
							// If it's a virtual stream, take the concrete parent stream instead
							HashSet<string> versionStreams = new HashSet<string>(Context.ProjectConfigFile.GetValues("Perforce.VersionStreams", Array.Empty<string>()), StringComparer.OrdinalIgnoreCase);
							while (!versionStreams.Contains(branchOrStreamName))
							{
								StreamRecord streamSpec = await perforce.GetStreamAsync(branchOrStreamName, false, cancellationToken);
								if (streamSpec.Type != "virtual" || streamSpec.Parent == "none" || streamSpec.Parent == null)
								{
									break;
								}
								branchOrStreamName = streamSpec.Parent;
							}
						}
						else
						{
							// Otherwise use the depot path for GenerateProjectFiles.bat in the root of the workspace
							List<WhereRecord> files = await perforce.WhereAsync(project.ClientRootPath + "/GenerateProjectFiles.bat", cancellationToken).Where(x => !x.Unmap).ToListAsync(cancellationToken);
							if (files.Count != 1)
							{
								return (WorkspaceUpdateResult.FailedToSync, $"Couldn't determine branch name for {project.ClientFileName}.");
							}
							branchOrStreamName = PerforceUtils.GetClientOrDepotDirectoryName(files[0].DepotFile);
						}

						logger.LogInformation("");

						// Get the last code change
						int codeChangeNumber = Context.CodeChangeNumber ?? 0;
						if (codeChangeNumber == 0)
						{
							logger.LogInformation("Finding last code change for CL {Number}...", Context.ChangeNumber);

							// If we are syncing to a newer change than the last code change we found (and it is not the first sync in a workspace)
							// go head and use the last code change we found as the minimum change in our query
							int? minChangeNumber = null;
							if ((Context.ChangeNumber >= state.CurrentCodeChangeNumber) && (state.CurrentCodeChangeNumber > 0))
							{
								minChangeNumber = state.CurrentCodeChangeNumber;
							}

							string[] codeRules = Utility.GetCodeFilter(Context.ProjectConfigFile);

							try
							{
								await foreach (PerforceChangeDetails details in Utility.EnumerateChangeDetails(perforce, minChangeNumber, maxChangeNumber: Context.ChangeNumber, syncPaths, codeRules, cancellationToken))
								{
									if (details.ContainsCode)
									{
										codeChangeNumber = details.Number;
										break;
									}
								}
							}
							catch (EpicGames.Perforce.PerforceException)
							{
								logger.LogInformation("Falling back to the slow way of finding last code change for CL {Number}...", Context.ChangeNumber);

								await foreach (PerforceChangeDetails details in Utility.EnumerateChangeDetails(perforce, null, maxChangeNumber: Context.ChangeNumber, syncPaths, codeRules, cancellationToken))
								{
									if (details.ContainsCode)
									{
										codeChangeNumber = details.Number;
										break;
									}
								}
							}

							if (codeChangeNumber == 0)
							{
								return (WorkspaceUpdateResult.FailedToSync, $"Could not find any code changes before CL {Context.ChangeNumber}.");
							}

							logger.LogInformation("Using code CL {Number}", codeChangeNumber);
						}

						// Set the version change
						if (Context.ProjectConfigFile.GetValue("Options.VersionToLastCodeChange", true))
						{
							versionChangeNumber = codeChangeNumber;
						}
						else
						{
							versionChangeNumber = Context.ChangeNumber;
						}

						// Update the version files
						if (Context.ProjectConfigFile.GetValue("Options.UseFastModularVersioningV2", false))
						{
							bool isLicenseeVersion = await IsLicenseeVersion(perforce, project, cancellationToken);
							if (!await UpdateVersionFile(perforce, project.ClientRootPath + BuildVersionFileName, Context.ChangeNumber, text => UpdateBuildVersion(text, Context.ChangeNumber, versionChangeNumber, branchOrStreamName, isLicenseeVersion), logger, cancellationToken))
							{
								return (WorkspaceUpdateResult.FailedToSync, $"Failed to update {BuildVersionFileName}.");
							}
						}
						else if (Context.ProjectConfigFile.GetValue("Options.UseFastModularVersioning", false))
						{
							bool isLicenseeVersion = await IsLicenseeVersion(perforce, project, cancellationToken);
							if (!await UpdateVersionFile(perforce, project.ClientRootPath + BuildVersionFileName, Context.ChangeNumber, text => UpdateBuildVersion(text, Context.ChangeNumber, versionChangeNumber, branchOrStreamName, isLicenseeVersion), logger, cancellationToken))
							{
								return (WorkspaceUpdateResult.FailedToSync, $"Failed to update {BuildVersionFileName}");
							}

							Dictionary<string, string> versionHeaderStrings = new Dictionary<string, string>();
							versionHeaderStrings["#define ENGINE_IS_PROMOTED_BUILD"] = " (0)";
							versionHeaderStrings["#define BUILT_FROM_CHANGELIST"] = " 0";
							versionHeaderStrings["#define BRANCH_NAME"] = " \"" + branchOrStreamName.Replace('/', '+') + "\"";
							if (!await UpdateVersionFile(perforce, project.ClientRootPath + VersionHeaderFileName, versionHeaderStrings, Context.ChangeNumber, logger, cancellationToken))
							{
								return (WorkspaceUpdateResult.FailedToSync, $"Failed to update {VersionHeaderFileName}.");
							}
							if (!await UpdateVersionFile(perforce, project.ClientRootPath + ObjectVersionFileName, new Dictionary<string, string>(), Context.ChangeNumber, logger, cancellationToken))
							{
								return (WorkspaceUpdateResult.FailedToSync, $"Failed to update {ObjectVersionFileName}.");
							}
						}
						else
						{
							if (!await UpdateVersionFile(perforce, project.ClientRootPath + BuildVersionFileName, new Dictionary<string, string>(), Context.ChangeNumber, logger, cancellationToken))
							{
								return (WorkspaceUpdateResult.FailedToSync, $"Failed to update {BuildVersionFileName}");
							}

							Dictionary<string, string> versionStrings = new Dictionary<string, string>();
							versionStrings["#define ENGINE_VERSION"] = " " + versionChangeNumber.ToString();
							versionStrings["#define ENGINE_IS_PROMOTED_BUILD"] = " (0)";
							versionStrings["#define BUILT_FROM_CHANGELIST"] = " " + versionChangeNumber.ToString();
							versionStrings["#define BRANCH_NAME"] = " \"" + branchOrStreamName.Replace('/', '+') + "\"";
							if (!await UpdateVersionFile(perforce, project.ClientRootPath + VersionHeaderFileName, versionStrings, Context.ChangeNumber, logger, cancellationToken))
							{
								return (WorkspaceUpdateResult.FailedToSync, $"Failed to update {VersionHeaderFileName}");
							}
							if (!await UpdateVersionFile(perforce, project.ClientRootPath + ObjectVersionFileName, versionStrings, Context.ChangeNumber, logger, cancellationToken))
							{
								return (WorkspaceUpdateResult.FailedToSync, $"Failed to update {ObjectVersionFileName}");
							}
						}
					}

					// Check if there are any files which need resolving
					List<FStatRecord> unresolvedFiles = await FindUnresolvedFiles(perforce, syncPaths, cancellationToken);
					if (unresolvedFiles.Count > 0 && Context.Options.HasFlag(WorkspaceUpdateOptions.AutoResolveChanges))
					{
						foreach (FStatRecord unresolvedFile in unresolvedFiles)
						{
							if (unresolvedFile.DepotFile != null)
							{
								await perforce.ResolveAsync(-1, ResolveOptions.Automatic, unresolvedFile.DepotFile, cancellationToken);
							}
						}
						unresolvedFiles = await FindUnresolvedFiles(perforce, syncPaths, cancellationToken);
					}
					if (unresolvedFiles.Count > 0)
					{
						logger.LogInformation("{NumFiles} files need resolving:", unresolvedFiles.Count);
						foreach (FStatRecord unresolvedFile in unresolvedFiles)
						{
							logger.LogInformation("  {ClientFile}", unresolvedFile.ClientFile);
						}
						return (WorkspaceUpdateResult.FilesToResolve, "Files need resolving.");
					}

					// Continue processing sync-only actions
					if (Context.Options.HasFlag(WorkspaceUpdateOptions.Sync) && !Context.Options.HasFlag(WorkspaceUpdateOptions.UpdateFilter))
					{
						Context.ProjectConfigFile ??= await ReadProjectConfigFile(project.LocalRootPath, project.LocalFileName, logger);

						// Execute any project specific post-sync steps
						string[]? postSyncSteps = Context.ProjectConfigFile.GetValues("Sync.Step", null);
						if (postSyncSteps != null)
						{
							logger.LogInformation("");
							logger.LogInformation("Executing post-sync steps...");

							Dictionary<string, string> postSyncVariables = ConfigUtils.GetWorkspaceVariables(project, Context.ChangeNumber, versionChangeNumber, null, Context.ProjectConfigFile, perforceSettings);
							foreach (string postSyncStep in postSyncSteps.Select(x => x.Trim()))
							{
								ConfigObject postSyncStepObject = new ConfigObject(postSyncStep);

								string toolFileName = Utility.ExpandVariables(postSyncStepObject.GetValue("FileName", ""), postSyncVariables);
								if (toolFileName != null)
								{
									string toolArguments = Utility.ExpandVariables(postSyncStepObject.GetValue("Arguments", ""), postSyncVariables);

									logger.LogInformation("post-sync> Running {FileName} {Arguments}", toolFileName, toolArguments);

									if (!File.Exists(toolFileName))
									{
										return (WorkspaceUpdateResult.FailedToSync, $"Unable to find {toolFileName}. You may have an incomplete sync; try cleaning your workspace.");
									}

									int resultFromTool = await Utility.ExecuteProcessAsync(toolFileName, null, toolArguments, line => ProcessOutput(line, "post-sync> ", Progress, logger), cancellationToken);
									if (resultFromTool != 0)
									{
										return (WorkspaceUpdateResult.FailedToSync, $"Post-sync step terminated with exit code {resultFromTool}.");
									}
								}
							}
						}
					}

					// Update the current state
					state = stateMgr.Modify(x =>
					{
						if (Context.Options.HasFlag(WorkspaceUpdateOptions.SyncSingleChange))
						{
							x.AdditionalChangeNumbers.Add(Context.ChangeNumber);
						}
						else
						{
							x.CurrentChangeNumber = Context.ChangeNumber;
							x.CurrentCodeChangeNumber = versionChangeNumber;
						}
					});

					// Update the timing info
					times.Add(new Tuple<string, TimeSpan>("Sync", syncTelemetryStopwatch.Stop("Success")));

					// Save the number of files synced
					numFilesSynced = syncDepotPaths.Count;
					logger.LogInformation("");
				}
			}

			// Extract an archive from the depot path
			if (Context.Options.HasFlag(WorkspaceUpdateOptions.SyncArchives))
			{
				using (TelemetryStopwatch stopwatch = new TelemetryStopwatch("Workspace_SyncArchives", project.TelemetryProjectIdentifier))
				{
					// Create the directory for extracted archive manifests
					DirectoryReference manifestDirectoryName;
					if (project.LocalFileName.HasExtension(".uproject"))
					{
						manifestDirectoryName = DirectoryReference.Combine(project.LocalFileName.Directory, "Saved", "UnrealGameSync");
					}
					else
					{
						manifestDirectoryName = DirectoryReference.Combine(project.LocalFileName.Directory, "Engine", "Saved", "UnrealGameSync");
					}
					DirectoryReference.CreateDirectory(manifestDirectoryName);

					// Sync and extract (or just remove) the given archives
					foreach ((string archiveType, IArchive? archive) in Context.ArchiveTypeToArchive)
					{
						// Remove any existing binaries
						FileReference manifestFileName = FileReference.Combine(manifestDirectoryName, String.Format("{0}.zipmanifest", archiveType));
						if (FileReference.Exists(manifestFileName))
						{
							bool isNotLastSyncedEditorArchive = archiveType != IArchiveChannel.EditorArchiveType || (archive != null && archive.Key != state.LastSyncEditorArchive);
							if (isNotLastSyncedEditorArchive)
							{
								logger.LogInformation("Removing {ArchiveType} binaries...", archiveType);
								Progress.Set(String.Format("Removing {0} binaries...", archiveType), 0.0f);
								ArchiveUtils.RemoveExtractedFiles(project.LocalRootPath, manifestFileName, Progress, logger);
								FileReference.Delete(manifestFileName);
								logger.LogInformation("");
							}
						}

						// If we have a new depot path, sync it down and extract it
						if (archive != null)
						{
							string archiveKey = archive.Key;
							if (archiveType != IArchiveChannel.EditorArchiveType || archiveKey != state.LastSyncEditorArchive)
							{
								logger.LogInformation("Syncing {ArchiveType} binaries...", archiveType);
								Progress.Set(String.Format("Syncing {0} binaries...", archiveType), 0.0f);
								if (!await archive.DownloadAsync(perforce, project.LocalRootPath, manifestFileName, logger, Progress, CancellationToken.None))
								{
									return (WorkspaceUpdateResult.FailedToSync, $"Couldn't read {archiveKey}");
								}
								// Update last synced editor archive
								if (archiveType == IArchiveChannel.EditorArchiveType)
								{
									state = stateMgr.Modify(x =>
									{
										x.LastSyncEditorArchive = archiveKey;
									});
								}
							}
							else
							{
								logger.LogInformation("Skipping {ArchiveType} binaries download, already downloaded", IArchiveChannel.EditorArchiveType);
							}
						}
					}

					// Update the state
					state = stateMgr.Modify(x =>
					{
						x.ExpandedArchiveTypes.Clear();
						x.ExpandedArchiveTypes.UnionWith(Context.ArchiveTypeToArchive.Where(x => x.Value != null).Select(x => x.Key));
					});

					// Add the finish time
					times.Add(new Tuple<string, TimeSpan>("Archive", stopwatch.Stop("Success")));
				}
			}

			// Take the lock before doing anything else. Building and generating project files can only be done on one workspace at a time.
			using SemaphoreScope scope = new SemaphoreScope(_updateSemaphore);
			if (Context.Options.HasFlag(WorkspaceUpdateOptions.GenerateProjectFiles) || Context.Options.HasFlag(WorkspaceUpdateOptions.Build))
			{
				if (!scope.TryAcquire())
				{
					logger.LogInformation("Waiting for other workspaces to finish...");
					await scope.AcquireAsync(cancellationToken);
				}
			}

			// Generate project files in the workspace
			if (Context.Options.HasFlag(WorkspaceUpdateOptions.GenerateProjectFiles))
			{
				using (TelemetryStopwatch stopwatch = new TelemetryStopwatch("Workspace_GenerateProjectFiles", project.TelemetryProjectIdentifier))
				{
					Progress.Set("Generating project files...", 0.0f);

					StringBuilder commandLine = new StringBuilder();
					commandLine.AppendFormat("\"{0}\"", FileReference.Combine(project.LocalRootPath, $"GenerateProjectFiles.{ShellScriptExt}"));
					if ((Context.Options & WorkspaceUpdateOptions.SyncAllProjects) == 0 && (Context.Options & WorkspaceUpdateOptions.IncludeAllProjectsInSolution) == 0)
					{
						if (project.LocalFileName.HasExtension(".uproject"))
						{
							commandLine.AppendFormat(" \"{0}\"", project.LocalFileName);
						}
					}
					commandLine.Append(" -progress");
					logger.LogInformation("Generating project files...");
					logger.LogInformation("gpf> Running {Arguments}", commandLine);

					int generateProjectFilesResult = await ExecuteShellCommandAsync(commandLine.ToString(), line => ProcessOutput(line, "gpf> ", Progress, logger), cancellationToken);
					if (generateProjectFilesResult != 0)
					{
						return (WorkspaceUpdateResult.FailedToCompile, $"Failed to generate project files (exit code {generateProjectFilesResult}).");
					}

					logger.LogInformation("");
					times.Add(new Tuple<string, TimeSpan>("Prj gen", stopwatch.Stop("Success")));
				}
			}

			// Build everything using MegaXGE
			if (Context.Options.HasFlag(WorkspaceUpdateOptions.Build))
			{
				Context.ProjectConfigFile ??= await ReadProjectConfigFile(project.LocalRootPath, project.LocalFileName, logger);

				// Figure out the new editor target
				TargetReceipt defaultEditorReceipt = ConfigUtils.CreateDefaultEditorReceipt(project, Context.ProjectConfigFile, Context.EditorConfig);

				FileReference editorTargetFile = ConfigUtils.GetEditorTargetFile(project, Context.ProjectConfigFile);
				string editorTargetName = editorTargetFile.GetFileNameWithoutAnyExtensions();
				FileReference editorReceiptFile = ConfigUtils.GetReceiptFile(project, Context.ProjectConfigFile, editorTargetFile, Context.EditorConfig.ToString());

				// Get the build steps
				bool usingPrecompiledEditor = Context.ArchiveTypeToArchive.TryGetValue(IArchiveChannel.EditorArchiveType, out IArchive? archive) && archive != null;
				Dictionary<Guid, ConfigObject> buildStepObjects = ConfigUtils.GetDefaultBuildStepObjects(project, editorTargetName, Context.EditorConfig, Context.ProjectConfigFile, usingPrecompiledEditor);
				BuildStep.MergeBuildStepObjects(buildStepObjects, Context.ProjectConfigFile.GetValues("Build.Step", Array.Empty<string>()).Select(x => new ConfigObject(x)));
				BuildStep.MergeBuildStepObjects(buildStepObjects, Context.UserBuildStepObjects);

				// Construct build steps from them
				List<BuildStep> buildSteps = buildStepObjects.Values.Select(x => new BuildStep(x)).OrderBy(x => (x.OrderIndex == -1) ? 10000 : x.OrderIndex).ToList();
				if (Context.CustomBuildSteps != null && Context.CustomBuildSteps.Count > 0)
				{
					buildSteps.RemoveAll(x => !Context.CustomBuildSteps.Contains(x.UniqueId));
				}
				else if (Context.Options.HasFlag(WorkspaceUpdateOptions.ScheduledBuild))
				{
					buildSteps.RemoveAll(x => !x.ScheduledSync);
				}
				else
				{
					buildSteps.RemoveAll(x => !x.NormalSync);
				}

				// Check if the last successful build was before a change that we need to force a clean for
				bool forceClean = false;
				if (state.LastBuiltChangeNumber != 0)
				{
					foreach (string cleanBuildChange in Context.ProjectConfigFile.GetValues("ForceClean.Changelist", Array.Empty<string>()))
					{
						int changeNumber;
						if (Int32.TryParse(cleanBuildChange, out changeNumber))
						{
							if ((state.LastBuiltChangeNumber >= changeNumber) != (state.CurrentChangeNumber >= changeNumber))
							{
								logger.LogInformation("Forcing clean build due to changelist {Change}.", changeNumber);
								logger.LogInformation("");
								forceClean = true;
								break;
							}
						}
					}
				}

				// Execute them all
				using (TelemetryStopwatch stopwatch = new TelemetryStopwatch("Workspace_Build", project.TelemetryProjectIdentifier))
				{
					Progress.Set("Starting build...", 0.0f);

					// Execute all the steps
					float maxProgressFraction = 0.0f;
					foreach (BuildStep step in buildSteps)
					{
						maxProgressFraction += (float)step.EstimatedDuration / (float)Math.Max(buildSteps.Sum(x => x.EstimatedDuration), 1);

						Progress.Set(step.StatusText ?? "Executing build step");
						Progress.Push(maxProgressFraction);

						logger.LogInformation("{Status}", step.StatusText);

						DirectoryReference batchFilesDir = DirectoryReference.Combine(project.LocalRootPath, "Engine", "Build", "BatchFiles");
						if (RuntimeInformation.IsOSPlatform(OSPlatform.OSX))
						{
							batchFilesDir = DirectoryReference.Combine(batchFilesDir, "Mac");
						}
						else if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
						{
							batchFilesDir = DirectoryReference.Combine(batchFilesDir, "Linux");
						}

						if (step.IsValid())
						{
							// Get the build variables for this step
							TargetReceipt? editorReceipt;
							if (!ConfigUtils.TryReadEditorReceipt(project, editorReceiptFile, out editorReceipt))
							{
								editorReceipt = defaultEditorReceipt;
							}
							Dictionary<string, string> variables = ConfigUtils.GetWorkspaceVariables(project, state.CurrentChangeNumber, state.CurrentCodeChangeNumber, editorReceipt, Context.ProjectConfigFile, perforceSettings, Context.AdditionalVariables);

							// Handle all the different types of steps
							switch (step.Type)
							{
								case BuildStepType.Compile:
									using (TelemetryStopwatch stepStopwatch = new TelemetryStopwatch("Workspace_Execute_Compile", project.TelemetryProjectIdentifier))
									{
										stepStopwatch.AddData(new { Target = step.Target });

										FileReference buildBat = FileReference.Combine(batchFilesDir, $"Build.{ShellScriptExt}");

										string commandLine = $"\"{buildBat}\" {step.Target} {step.Platform} {step.Configuration} {Utility.ExpandVariables(step.Arguments ?? "", variables)} -NoHotReloadFromIDE";
										if (Context.Options.HasFlag(WorkspaceUpdateOptions.Clean) || forceClean)
										{
											logger.LogInformation("ubt> Running {Arguments}", commandLine + " -clean");
											await ExecuteShellCommandAsync(commandLine + " -clean", line => ProcessOutput(line, "ubt> ", Progress, logger), cancellationToken);
										}

										logger.LogInformation("ubt> Running {FileName} {Arguments}", buildBat, commandLine + " -progress");

										int resultFromBuild = await ExecuteShellCommandAsync(commandLine + " -progress", line => ProcessOutput(line, "ubt> ", Progress, logger), cancellationToken);
										if (resultFromBuild != 0)
										{
											stepStopwatch.Stop("Failed");

											WorkspaceUpdateResult result;
											if (await HasModifiedSourceFiles(perforce, project, cancellationToken) || Context.UserBuildStepObjects.Count > 0)
											{
												result = WorkspaceUpdateResult.FailedToCompile;
											}
											else
											{
												result = WorkspaceUpdateResult.FailedToCompileWithCleanWorkspace;
											}

											return (result, $"Failed to compile {step.Target}");
										}

										stepStopwatch.Stop("Success");
									}
									break;
								case BuildStepType.Cook:
									using (TelemetryStopwatch stepStopwatch = new TelemetryStopwatch("Workspace_Execute_Cook", project.TelemetryProjectIdentifier))
									{
										stepStopwatch.AddData(new { Project = Path.GetFileNameWithoutExtension(step.FileName) });

										FileReference localRunUat = FileReference.Combine(batchFilesDir, $"RunUAT.{ShellScriptExt}");
										string arguments = String.Format("\"{0}\" -profile=\"{1}\"", localRunUat, FileReference.Combine(project.LocalRootPath, step.FileName ?? "unknown"));
										logger.LogInformation("uat> Running {FileName} {Argument}", localRunUat, arguments);

										int resultFromUat = await ExecuteShellCommandAsync(arguments, line => ProcessOutput(line, "uat> ", Progress, logger), cancellationToken);
										if (resultFromUat != 0)
										{
											stepStopwatch.Stop("Failed");
											return (WorkspaceUpdateResult.FailedToCompile, $"Cook failed. ({resultFromUat})");
										}

										stepStopwatch.Stop("Success");
									}
									break;
								case BuildStepType.Other:
									using (TelemetryStopwatch stepStopwatch = new TelemetryStopwatch("Workspace_Execute_Custom", project.TelemetryProjectIdentifier))
									{
										stepStopwatch.AddData(new { FileName = Path.GetFileNameWithoutExtension(step.FileName) });

										FileReference toolFileName = FileReference.Combine(project.LocalRootPath, Utility.ExpandVariables(step.FileName ?? "unknown", variables));
										string toolWorkingDir = String.IsNullOrWhiteSpace(step.WorkingDir) ? toolFileName.Directory.FullName : Utility.ExpandVariables(step.WorkingDir, variables);
										string toolArguments = Utility.ExpandVariables(step.Arguments ?? "", variables);
										logger.LogInformation("tool> Running {Tool} {Arguments}", toolFileName, toolArguments);

										if (!FileReference.Exists(toolFileName))
										{
											logger.LogInformation("Unable to find {FileName}. You may have an incomplete sync; try cleaning your workspace.", toolFileName);
											stepStopwatch.Stop("Failed");
											return (WorkspaceUpdateResult.FailedToCompile, $"Unable to run {toolFileName}");
										}

										if (step.UseLogWindow)
										{
											int resultFromTool = await Utility.ExecuteProcessAsync(toolFileName.FullName, toolWorkingDir, toolArguments, line => ProcessOutput(line, "tool> ", Progress, logger), cancellationToken);
											if (resultFromTool != 0)
											{
												stepStopwatch.Stop("Failed");
												return (WorkspaceUpdateResult.FailedToCompile, $"Tool terminated with exit code {resultFromTool}.");
											}
										}
										else
										{
											ProcessStartInfo startInfo = new ProcessStartInfo(toolFileName.FullName, toolArguments);
											startInfo.WorkingDirectory = toolWorkingDir;
											using (Process.Start(startInfo))
											{
											}
										}

										stepStopwatch.Stop("Success");
									}
									break;
							}
						}

						logger.LogInformation("");
						Progress.Pop();
					}

					times.Add(new Tuple<string, TimeSpan>("Build", stopwatch.Stop("Success")));
				}

				// Update the last successful build change number
				if (Context.CustomBuildSteps == null || Context.CustomBuildSteps.Count == 0)
				{
					state = stateMgr.Modify(x => x.LastBuiltChangeNumber = state.CurrentChangeNumber);
				}
			}

			// Write out all the timing information
			logger.LogInformation("Total time: {TotalTime}", FormatTime(times.Sum(x => (long)(x.Item2.TotalMilliseconds / 1000))));
			foreach (Tuple<string, TimeSpan> time in times)
			{
				logger.LogInformation("   {Name,-8}: {TimeSeconds}", time.Item1, FormatTime((long)(time.Item2.TotalMilliseconds / 1000)));
			}
			if (numFilesSynced > 0)
			{
				logger.LogInformation("{NumFiles} files synced.", numFilesSynced);
			}

			DateTime finishTime = DateTime.Now;
			logger.LogInformation("");
			logger.LogInformation("UPDATE SUCCEEDED ({FinishDate} {FinishTime})", finishTime.ToShortDateString(), finishTime.ToShortTimeString());

			return (WorkspaceUpdateResult.Success, "Update succeeded");
		}

		static void ProcessOutput(string line, string prefix, ProgressValue progress, ILogger logger)
		{
			string? parsedLine = ProgressTextWriter.ParseLine(line, progress);
			if (parsedLine != null)
			{
				logger.LogInformation("{Prefix}{Message}", prefix, parsedLine);
			}
		}

		static async Task<bool> IsLicenseeVersion(IPerforceConnection perforce, ProjectInfo project, CancellationToken cancellationToken)
		{
			string[] files =
			{
				project.ClientRootPath + "/Engine/Build/NotForLicensees/EpicInternal.txt",
				project.ClientRootPath + "/Engine/Restricted/NotForLicensees/Build/EpicInternal.txt"
			};

			List<FStatRecord> records = await perforce.FStatAsync(files, cancellationToken).ToListAsync(cancellationToken);
			return !records.Any(x => x.IsMapped);
		}

		public static List<string> GetSyncPaths(ProjectInfo project, bool syncAllProjects, string[] syncFilter)
		{
			List<string> syncPaths = GetRelativeSyncPaths(project, syncAllProjects, syncFilter);
			return syncPaths.Select(x => project.ClientRootPath + x).ToList();
		}

		public static List<string> GetRelativeSyncPaths(ProjectInfo project, bool syncAllProjects, IReadOnlyList<string>? syncFilter)
		{
			List<string> syncPaths = new List<string>();

			// Check the client path is formatted correctly
			if (!project.ClientFileName.StartsWith(project.ClientRootPath + "/", StringComparison.OrdinalIgnoreCase))
			{
				throw new Exception(String.Format("Expected '{0}' to start with '{1}'", project.ClientFileName, project.ClientRootPath));
			}

			// Add the default project paths
			int lastSlashIdx = project.ClientFileName.LastIndexOf('/');
			if (syncAllProjects || !project.ClientFileName.EndsWith(".uproject", StringComparison.OrdinalIgnoreCase) || lastSlashIdx <= project.ClientRootPath.Length)
			{
				syncPaths.Add("/...");
			}
			else
			{
				syncPaths.Add("/*");
				syncPaths.Add("/Engine/...");
				if (project.IsEnterpriseProject)
				{
					syncPaths.Add("/Enterprise/...");
				}
				syncPaths.Add(project.ClientFileName.Substring(project.ClientRootPath.Length, lastSlashIdx - project.ClientRootPath.Length) + "/...");
			}

			// Apply the sync filter to that list. We only want inclusive rules in the output list, but we can manually apply exclusions to previous entries.
			if (syncFilter != null)
			{
				foreach (string syncPath in syncFilter)
				{
					string trimSyncPath = syncPath.Trim();
					if (trimSyncPath.StartsWith("/", StringComparison.Ordinal))
					{
						syncPaths.Add(trimSyncPath);
					}
					else if (trimSyncPath.StartsWith("-/", StringComparison.Ordinal) && trimSyncPath.EndsWith("...", StringComparison.Ordinal))
					{
						syncPaths.RemoveAll(x => x.StartsWith(trimSyncPath.Substring(1, trimSyncPath.Length - 4), StringComparison.Ordinal));
					}
				}
			}

			// Sort the remaining paths by length, and remove any paths which are included twice
			syncPaths = syncPaths.OrderBy(x => x.Length).ToList();
			for (int idx = 0; idx < syncPaths.Count; idx++)
			{
				string syncPath = syncPaths[idx];
				if (syncPath.EndsWith("...", StringComparison.Ordinal))
				{
					string syncPathPrefix = syncPath.Substring(0, syncPath.Length - 3);
					for (int otherIdx = syncPaths.Count - 1; otherIdx > idx; otherIdx--)
					{
						if (syncPaths[otherIdx].StartsWith(syncPathPrefix, StringComparison.Ordinal))
						{
							syncPaths.RemoveAt(otherIdx);
						}
					}
				}
			}

			return syncPaths;
		}

		public static bool MatchFilter(ProjectInfo project, FileReference fileName, FileFilter filter)
		{
			bool match = true;
			if (fileName.IsUnderDirectory(project.LocalRootPath))
			{
				string relativePath = fileName.MakeRelativeTo(project.LocalRootPath);
				if (!filter.Matches(relativePath))
				{
					match = false;
				}
			}
			return match;
		}

		class SyncState
		{
			public int TotalDepotPaths;
			public HashSet<string> RemainingDepotPaths;
			public Queue<List<string>> SyncCommandLists;
			public string StatusMessage;
			public WorkspaceUpdateResult Result = WorkspaceUpdateResult.Success;

			public SyncState(HashSet<string> remainingDepotPaths, Queue<List<string>> syncCommandLists)
			{
				TotalDepotPaths = remainingDepotPaths.Count;
				RemainingDepotPaths = remainingDepotPaths;
				SyncCommandLists = syncCommandLists;
				StatusMessage = "Succeeded.";
			}
		}

		static Task<(WorkspaceUpdateResult, string)> SyncFileRevisions(IPerforceConnection perforce, string prefix, WorkspaceUpdateContext context, List<string> syncCommands, HashSet<string> remainingDepotPaths, ProgressValue progress, ILogger logger, CancellationToken cancellationToken)
		{
			Queue<List<string>> syncCommandLists = new Queue<List<string>>();
			foreach (IReadOnlyList<string> batch in syncCommands.Batch(2000))
			{
				syncCommandLists.Enqueue(batch.ToList());
			}
			return SyncFileRevisions(perforce, prefix, context, syncCommandLists, remainingDepotPaths, progress, logger, cancellationToken);
		}

		static async Task<(WorkspaceUpdateResult, string)> SyncFileRevisions(IPerforceConnection perforce, string prefix, WorkspaceUpdateContext context, Queue<List<string>> syncCommandLists, HashSet<string> remainingDepotPaths, ProgressValue progress, ILogger logger, CancellationToken cancellationToken)
		{
			// Figure out the number of additional background threads we want to run with. We can run worker on the current thread.
			int numThreads = context.PerforceSyncOptions?.NumThreads ?? PerforceSyncOptions.DefaultNumThreads;
			int numExtraThreads = Math.Max(Math.Min(syncCommandLists.Count, numThreads) - 1, 0);

			List<IPerforceConnection> childConnections = new List<IPerforceConnection>();
			List<Task> childTasks = new List<Task>(numExtraThreads);
			try
			{
				// Create the state object shared by all the worker threads
				SyncState state = new SyncState(remainingDepotPaths, syncCommandLists);

				// Wrapper writer around the log class to prevent multiple threads writing to it at once
				ILogger logWrapper = logger;

				// Initialize Sync Progress
				UpdateSyncState(prefix, state, progress);

				// Delegate for updating the sync state after a file has been synced
				Action<SyncRecord, ILogger> syncOutput = (record, localLog) => { UpdateSyncState(prefix, record, state, progress, localLog); };

				// Create all the child threads
				for (int threadIdx = 0; threadIdx < numExtraThreads; threadIdx++)
				{
					int threadNumber = threadIdx + 2;

					// Create connection
					IPerforceConnection childConnection = await PerforceConnection.CreateAsync(perforce.Settings, perforce.Logger);
					childConnections.Add(childConnection);

					Task childTask = Task.Run(() => StaticSyncWorker(threadNumber, childConnection, context, state, syncOutput, logWrapper, cancellationToken), cancellationToken);
					childTasks.Add(childTask);
				}

				// Run one worker on the current thread
				await StaticSyncWorker(1, perforce, context, state, syncOutput, logWrapper, cancellationToken);

				// Allow the tasks to throw
				foreach (Task childTask in childTasks)
				{
					await childTask;
				}

				// Return the result that was set on the state object
				return (state.Result, state.StatusMessage);
			}
			finally
			{
				foreach (Task childTask in childTasks)
				{
					await childTask.ContinueWith(_ => { }, TaskScheduler.Default); // Swallow exceptions until we've disposed the connections
				}

				foreach (IPerforceConnection childConnection in childConnections)
				{
					childConnection.Dispose();
				}
			}
		}

		static void UpdateSyncState(string prefix, SyncState state, ProgressValue progress)
		{
			lock (state)
			{
				string message = String.Format("{0} ({1:n0}/{2:n0})", prefix, state.TotalDepotPaths - state.RemainingDepotPaths.Count, state.TotalDepotPaths);
				float fraction = Math.Min((float)(state.TotalDepotPaths - state.RemainingDepotPaths.Count) / (float)state.TotalDepotPaths, 1.0f);
				progress.Set(message, fraction);
			}
		}

		static void UpdateSyncState(string prefix, SyncRecord record, SyncState state, ProgressValue progress, ILogger logger)
		{
			lock (state)
			{
				state.RemainingDepotPaths.Remove(record.DepotFile.ToString());

				string message = String.Format("{0} ({1:n0}/{2:n0})", prefix, state.TotalDepotPaths - state.RemainingDepotPaths.Count, state.TotalDepotPaths);
				float fraction = Math.Min((float)(state.TotalDepotPaths - state.RemainingDepotPaths.Count) / (float)state.TotalDepotPaths, 1.0f);
				progress.Set(message, fraction);

				logger.LogInformation("p4>   {Action} {Path}", record.Action, record.Path);
			}
		}

		static async Task StaticSyncWorker(int threadNumber, IPerforceConnection perforce, WorkspaceUpdateContext context, SyncState state, Action<SyncRecord, ILogger> syncOutput, ILogger globalLog, CancellationToken cancellationToken)
		{
			PrefixedTextWriter threadLog = new PrefixedTextWriter(String.Format("{0}:", threadNumber), globalLog);
			for (; ; )
			{
				// Remove the next batch that needs to be synced
				List<string> syncCommands;
				lock (state)
				{
					if (state.Result == WorkspaceUpdateResult.Success && state.SyncCommandLists.Count > 0)
					{
						syncCommands = state.SyncCommandLists.Dequeue();
					}
					else
					{
						break;
					}
				}

				WorkspaceUpdateResult result = WorkspaceUpdateResult.FailedToSync;
				string statusMessage = "";

				int maxRetries = context.PerforceSyncOptions?.NumSyncErrorRetries ?? PerforceSyncOptions.DefaultNumSyncErrorRetries;
				for (int attempt = 0; ; attempt++)
				{
					// Sync the files
					string? errorMessage;
					(result, statusMessage, errorMessage) = await StaticSyncFileRevisions(perforce, context, syncCommands, record => syncOutput(record, threadLog), cancellationToken);
					if (result != WorkspaceUpdateResult.FailedToSync || attempt >= maxRetries)
					{
						break;
					}
					threadLog.LogWarning("Sync error ({Message}); retrying... ({Count}/{MaxCount})", errorMessage ?? "unknown", attempt + 1, maxRetries);
				}

				// If it failed, try to set it on the state if nothing else has failed first
				if (result != WorkspaceUpdateResult.Success)
				{
					lock (state)
					{
						if (state.Result == WorkspaceUpdateResult.Success)
						{
							state.Result = result;
							state.StatusMessage = statusMessage;
						}
					}
					break;
				}
			}
		}

		static async Task<(WorkspaceUpdateResult, string, string?)> StaticSyncFileRevisions(IPerforceConnection perforce, WorkspaceUpdateContext context, List<string> syncCommands, Action<SyncRecord> syncOutput, CancellationToken cancellationToken)
		{
			// Sync them all. Explicitly disable parallel syncing here to avoid shelling out to p4.exe.
			List<PerforceResponse<SyncRecord>> responses = await perforce.TrySyncAsync(SyncOptions.None, -1, 0, -1, -1, -1, -1, syncCommands, cancellationToken).ToListAsync(cancellationToken);

			List<string> tamperedFiles = new List<string>();
			foreach (PerforceResponse<SyncRecord> response in responses)
			{
				const string noClobberPrefix = "Can't clobber writable file ";
				if (response.Info != null)
				{
					continue;
				}
				else if (response.Succeeded)
				{
					syncOutput(response.Data);
				}
				else if (response.Error != null && response.Error.Generic == PerforceGenericCode.Client && response.Error.Data.StartsWith(noClobberPrefix, StringComparison.OrdinalIgnoreCase))
				{
					tamperedFiles.Add(response.Error.Data.Substring(noClobberPrefix.Length).Trim());
				}
				else
				{
					return (WorkspaceUpdateResult.FailedToSync, $"Aborted sync due to error ({response}). If you are on an unreliable connection, you may wish to increase the number of retries from Options > Application Settings... > Advanced.", response.ToString());
				}
			}

			// If any files need to be clobbered, defer to the main thread to figure out which ones
			if (tamperedFiles.Count > 0)
			{
				if ((context.Options & WorkspaceUpdateOptions.Clobber) == 0)
				{
					int numNewFilesToClobber = 0;
					foreach (string tamperedFile in tamperedFiles)
					{
						if (!context.ClobberFiles.ContainsKey(tamperedFile))
						{
							context.ClobberFiles[tamperedFile] = true;
							if (tamperedFile.EndsWith(LocalObjectVersionFileName, StringComparison.OrdinalIgnoreCase) || tamperedFile.EndsWith(LocalVersionHeaderFileName, StringComparison.OrdinalIgnoreCase))
							{
								// Hack for UseFastModularVersioningV2; we don't need to update these files any more.
								continue;
							}
							numNewFilesToClobber++;
						}
					}
					if (numNewFilesToClobber > 0)
					{
						return (WorkspaceUpdateResult.FilesToClobber, $"Cancelled sync after checking files to clobber ({numNewFilesToClobber} new files).", null);
					}
				}
				foreach (string tamperedFile in tamperedFiles)
				{
					bool shouldClobber = (context.Options & WorkspaceUpdateOptions.Clobber) != 0 || context.ClobberFiles[tamperedFile];
					if (shouldClobber)
					{
						List<PerforceResponse<SyncRecord>> response = await perforce.TrySyncAsync(SyncOptions.Force, -1, 0, -1, -1, -1, -1, tamperedFile, cancellationToken).ToListAsync(cancellationToken);
						if (!response.Succeeded())
						{
							return (WorkspaceUpdateResult.FailedToSync, $"Couldn't sync {tamperedFile}.", response.ToString());
						}
					}
				}
			}

			// All succeeded
			return (WorkspaceUpdateResult.Success, "Succeeded.", null);
		}

		public static async Task<ConfigFile> ReadProjectConfigFile(DirectoryReference localRootPath, FileReference selectedLocalFileName, ILogger logger)
		{
			// Find the valid config file paths
			DirectoryInfo engineDir = DirectoryReference.Combine(localRootPath, "Engine").ToDirectoryInfo();
			List<FileInfo> localConfigFiles = Utility.GetLocalConfigPaths(engineDir, selectedLocalFileName.ToFileInfo());

			// Read them in
			ConfigFile projectConfig = new ConfigFile();
			foreach (FileInfo localConfigFile in localConfigFiles)
			{
				try
				{
					string[] lines = await File.ReadAllLinesAsync(localConfigFile.FullName);
					projectConfig.Parse(lines);
					logger.LogDebug("Read config file from {FileName}", localConfigFile.FullName);
				}
				catch (Exception ex)
				{
					logger.LogWarning(ex, "Failed to read config file from {FileName}", localConfigFile.FullName);
				}
			}
			return projectConfig;
		}

		public static async Task<IReadOnlyList<string>?> ReadProjectStreamFilter(IPerforceConnection perforce, ConfigFile projectConfigFile, CancellationToken cancellationToken)
		{
			string? streamListDepotPath = projectConfigFile.GetValue("Options.QuickSelectStreamList", null);
			if (streamListDepotPath == null)
			{
				return null;
			}

			PerforceResponse<PrintRecord<string[]>> response = await perforce.TryPrintLinesAsync(streamListDepotPath, cancellationToken);
			if (!response.Succeeded)
			{
				return null;
			}

			return response.Data.Contents?.Select(x => x.Trim()).Where(x => x.Length > 0).ToList().AsReadOnly();
		}

		static string FormatTime(long seconds)
		{
			if (seconds >= 60)
			{
				return String.Format("{0,3}m {1:00}s", seconds / 60, seconds % 60);
			}
			else
			{
				return String.Format("     {0,2}s", seconds);
			}
		}

		static async Task<bool> HasModifiedSourceFiles(IPerforceConnection perforce, ProjectInfo project, CancellationToken cancellationToken)
		{
			List<OpenedRecord> openFiles = await perforce.OpenedAsync(OpenedOptions.None, project.ClientRootPath + "/...", cancellationToken).ToListAsync(cancellationToken);
			if (openFiles.Any(x => x.DepotFile.Contains("/Source/", StringComparison.OrdinalIgnoreCase)))
			{
				return true;
			}
			return false;
		}

		static async Task<List<FStatRecord>> FindUnresolvedFiles(IPerforceConnection perforce, IEnumerable<string> syncPaths, CancellationToken cancellationToken)
		{
			List<FStatRecord> unresolvedFiles = new List<FStatRecord>();
			foreach (string syncPath in syncPaths)
			{
				List<FStatRecord> records = await perforce.FStatAsync(FStatOptions.OnlyUnresolved, syncPath, cancellationToken).ToListAsync(cancellationToken);
				unresolvedFiles.AddRange(records);
			}
			return unresolvedFiles;
		}

		static Task<bool> UpdateVersionFile(IPerforceConnection perforce, string clientPath, Dictionary<string, string> versionStrings, int changeNumber, ILogger logger, CancellationToken cancellationToken)
		{
			return UpdateVersionFile(perforce, clientPath, changeNumber, text => UpdateVersionStrings(text, versionStrings), logger, cancellationToken);
		}

		static async Task<bool> UpdateVersionFile(IPerforceConnection perforce, string clientPath, int changeNumber, Func<string, string> update, ILogger logger, CancellationToken cancellationToken)
		{
			List<PerforceResponse<FStatRecord>> records = await perforce.TryFStatAsync(FStatOptions.None, clientPath, cancellationToken).ToListAsync(cancellationToken);
			if (!records.Succeeded())
			{
				logger.LogInformation("Failed to query records for {ClientPath}", clientPath);
				return false;
			}
			if (records.Count > 1)
			{
				// Attempt to remove any existing file which is synced
				await perforce.SyncAsync(SyncOptions.Force, -1, $"{clientPath}#0", cancellationToken).ToListAsync(cancellationToken);

				// Try to get the mapped files again
				records = await perforce.TryFStatAsync(FStatOptions.None, clientPath, cancellationToken).ToListAsync(cancellationToken);
				if (!records.Succeeded())
				{
					logger.LogInformation("Failed to query records for {ClientPath}", clientPath);
					return false;
				}
			}
			if (records.Count == 0)
			{
				logger.LogInformation("Ignoring {ClientPath}; not found on server.", clientPath);
				return true;
			}

			FStatRecord record = records[0].Data;
			string? localPath = record.ClientFile; // Actually a filesystem path
			if (localPath == null)
			{
				logger.LogInformation("Version file is not mapped to workspace ({ClientFile})", clientPath);
				return false;
			}
			string? depotPath = record.DepotFile;
			if (depotPath == null)
			{
				logger.LogInformation("Version file does not exist in depot ({ClientFile})", clientPath);
				return false;
			}

			PerforceResponse<PrintRecord<string[]>> response = await perforce.TryPrintLinesAsync($"{depotPath}@{changeNumber}", cancellationToken);
			if (!response.Succeeded)
			{
				logger.LogInformation("Couldn't get default contents of {DepotPath}", depotPath);
				return false;
			}

			string[]? contents = response.Data.Contents;
			if (contents == null)
			{
				logger.LogInformation("No data returned for {DepotPath}", depotPath);
				return false;
			}

			string text = String.Join("\n", contents);
			text = update(text);
			return await WriteVersionFile(perforce, localPath, depotPath, text, logger, cancellationToken);
		}

		static string UpdateVersionStrings(string text, Dictionary<string, string> versionStrings)
		{
			using StringWriter writer = new StringWriter();
			foreach (string line in text.Split('\n'))
			{
				string newLine = line;
				foreach (KeyValuePair<string, string> versionString in versionStrings)
				{
					if (UpdateVersionLine(ref newLine, versionString.Key, versionString.Value))
					{
						break;
					}
				}
				writer.WriteLine(newLine);
			}
			return writer.ToString();
		}

		static string UpdateBuildVersion(string text, int changelist, int codeChangelist, string branchOrStreamName, bool isLicenseeVersion)
		{
			Dictionary<string, object> obj = JsonSerializer.Deserialize<Dictionary<string, object>>(text, Utility.DefaultJsonSerializerOptions)!;

			int prevCompatibleChangelist = 0;
			if (obj.TryGetValue("CompatibleChangelist", out object? prevCompatibleChangelistObj))
			{
				if (!Int32.TryParse(prevCompatibleChangelistObj?.ToString(), out prevCompatibleChangelist))
				{
					prevCompatibleChangelist = 0;
				}
			}

			int prevIsLicenseeVersion = 0;
			if (obj.TryGetValue("IsLicenseeVersion", out object? prevIsLicenseeVersionObj))
			{
				if (!Int32.TryParse(prevIsLicenseeVersionObj?.ToString(), out prevIsLicenseeVersion))
				{
					prevIsLicenseeVersion = 0;
				}
			}

			obj["Changelist"] = changelist;
			if (prevCompatibleChangelist == 0 || (prevIsLicenseeVersion != 0) != isLicenseeVersion)
			{
				// Don't overwrite the compatible changelist if we're in a hotfix release
				obj["CompatibleChangelist"] = codeChangelist;
			}
			obj["BranchName"] = branchOrStreamName.Replace('/', '+');
			obj["IsPromotedBuild"] = 0;
			obj["IsLicenseeVersion"] = isLicenseeVersion ? 1 : 0;

			return JsonSerializer.Serialize(obj, new JsonSerializerOptions
			{
				WriteIndented = true,
				// do not escape +
				Encoder = JavaScriptEncoder.UnsafeRelaxedJsonEscaping
			});
		}

		static async Task<bool> WriteVersionFile(IPerforceConnection perforce, string localPath, string depotPath, string newText, ILogger logger, CancellationToken cancellationToken)
		{
			try
			{
				if (File.Exists(localPath) && await File.ReadAllTextAsync(localPath, cancellationToken) == newText)
				{
					logger.LogInformation("Ignored {FileName}; contents haven't changed", localPath);
				}
				else
				{
					Directory.CreateDirectory(Path.GetDirectoryName(localPath)!);
					Utility.ForceDeleteFile(localPath);
					if (depotPath != null)
					{
						await perforce.SyncAsync(depotPath + "#0", cancellationToken).ToListAsync(cancellationToken);
					}
					await File.WriteAllTextAsync(localPath, newText, cancellationToken);
					logger.LogInformation("Written {FileName}", localPath);
				}
				return true;
			}
			catch (Exception ex)
			{
				logger.LogError(ex, "Failed to write to {FileName}.", localPath);
				return false;
			}
		}

		static bool UpdateVersionLine(ref string line, string prefix, string suffix)
		{
			int lineIdx = 0;
			int prefixIdx = 0;
			for (; ; )
			{
				string? prefixToken = ReadToken(prefix, ref prefixIdx);
				if (prefixToken == null)
				{
					break;
				}

				string? lineToken = ReadToken(line, ref lineIdx);
				if (lineToken == null || lineToken != prefixToken)
				{
					return false;
				}
			}
			line = line.Substring(0, lineIdx) + suffix;
			return true;
		}

		static string? ReadToken(string line, ref int lineIdx)
		{
			for (; ; lineIdx++)
			{
				if (lineIdx == line.Length)
				{
					return null;
				}
				else if (!Char.IsWhiteSpace(line[lineIdx]))
				{
					break;
				}
			}

			int startIdx = lineIdx++;
			if (Char.IsLetterOrDigit(line[startIdx]) || line[startIdx] == '_')
			{
				while (lineIdx < line.Length && (Char.IsLetterOrDigit(line[lineIdx]) || line[lineIdx] == '_'))
				{
					lineIdx++;
				}
			}

			return line.Substring(startIdx, lineIdx - startIdx);
		}

		public Tuple<string, float> CurrentProgress => Progress.Current;
	}
}
