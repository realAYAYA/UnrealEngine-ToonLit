// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Perforce;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.IO;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace UnrealGameSync
{
	interface IArchiveInfoSource
	{
		IReadOnlyList<IArchiveInfo> AvailableArchives { get; }
	}

	class PerforceMonitor : IDisposable, IArchiveInfoSource
	{

		class PerforceChangeSorter : IComparer<ChangesRecord>
		{
			public int Compare(ChangesRecord? summaryA, ChangesRecord? summaryB)
			{
				return summaryB!.Number - summaryA!.Number;
			}
		}

		public int InitialMaxChangesValue = 100;

		IPerforceSettings _perforceSettings;
		readonly string _branchClientPath;
		readonly string _selectedClientFileName;
		readonly string _selectedProjectIdentifier;
		Task? _workerTask;
		CancellationTokenSource _cancellationSource;
		int _pendingMaxChangesValue;
		SortedSet<ChangesRecord> _changes = new SortedSet<ChangesRecord>(new PerforceChangeSorter());
		SortedDictionary<int, PerforceChangeDetails> _changeDetails = new SortedDictionary<int,PerforceChangeDetails>();
		SortedSet<int> _promotedChangeNumbers = new SortedSet<int>();
		List<PerforceArchiveInfo> _archives = new List<PerforceArchiveInfo>();
		AsyncEvent _refreshEvent = new AsyncEvent();
		ILogger _logger;
		bool _isEnterpriseProject;
		DirectoryReference _cacheFolder;
		List<KeyValuePair<FileReference, DateTime>> _localConfigFiles;
		IAsyncDisposer _asyncDisposeTasks;

		SynchronizationContext _synchronizationContext;
		public event Action? OnUpdate;
		public event Action? OnUpdateMetadata;
		public event Action? OnStreamChange;
		public event Action? OnLoginExpired;

		public TimeSpan ServerTimeZone
		{
			get;
			private set;
		}

		public PerforceMonitor(IPerforceSettings inPerforceSettings, ProjectInfo projectInfo, ConfigFile inProjectConfigFile, DirectoryReference inCacheFolder, List<KeyValuePair<FileReference, DateTime>> inLocalConfigFiles, IServiceProvider inServiceProvider)
		{
			_perforceSettings = inPerforceSettings;
			_branchClientPath = projectInfo.ClientRootPath;
			_selectedClientFileName = projectInfo.ClientFileName;
			_selectedProjectIdentifier = projectInfo.ProjectIdentifier;
			_pendingMaxChangesValue = InitialMaxChangesValue;
			LastChangeByCurrentUser = -1;
			LastCodeChangeByCurrentUser = -1;
			_logger = inServiceProvider.GetRequiredService<ILogger<PerforceMonitor>>();
			_isEnterpriseProject = projectInfo.IsEnterpriseProject;
			LatestProjectConfigFile = inProjectConfigFile;
			_cacheFolder = inCacheFolder;
			_localConfigFiles = inLocalConfigFiles;
			_asyncDisposeTasks = inServiceProvider.GetRequiredService<IAsyncDisposer>();
			_synchronizationContext = SynchronizationContext.Current!;
			_cancellationSource = new CancellationTokenSource();

			AvailableArchives = (new List<IArchiveInfo>()).AsReadOnly();
		}

		public void Start()
		{
			if (_workerTask == null)
			{
				_workerTask = Task.Run(() => PollForUpdates(_cancellationSource.Token));
			}
		}

		public void Dispose()
		{
			OnUpdate = null;
			OnUpdateMetadata = null;
			OnStreamChange = null;
			OnLoginExpired = null;

			if (_workerTask != null)
			{
				_cancellationSource.Cancel();
				_asyncDisposeTasks.Add(_workerTask.ContinueWith(_ => _cancellationSource.Dispose()));
				_workerTask = null;
			}
		}

		public bool IsActive
		{
			get;
			set;
		}

		public string LastStatusMessage
		{
			get;
			private set;
		} = "";

		public int CurrentMaxChanges
		{
			get;
			private set;
		}

		public int PendingMaxChanges
		{
			get { return _pendingMaxChangesValue; }
			set { lock(this){ if(value != _pendingMaxChangesValue){ _pendingMaxChangesValue = value; _refreshEvent.Set(); } } }
		}

		async Task PollForUpdates(CancellationToken cancellationToken)
		{
			while (!cancellationToken.IsCancellationRequested)
			{
				try
				{
					await PollForUpdatesInner(cancellationToken);
				}
				catch (OperationCanceledException) when (cancellationToken.IsCancellationRequested)
				{
					break;
				}
				catch (Exception ex)
				{
					_logger.LogError(ex, "Unhandled exception in PollForUpdatesInner()");
					if (!(ex is PerforceException))
					{
						Program.CaptureException(ex);
					}
					await Task.Delay(TimeSpan.FromSeconds(20.0), cancellationToken).ContinueWith(x => { });
				}
			}
		}

		async Task PollForUpdatesInner(CancellationToken cancellationToken)
		{
			string? streamName;
			using (IPerforceConnection perforce = await PerforceConnection.CreateAsync(_perforceSettings, _logger))
			{
				streamName = await perforce.GetCurrentStreamAsync(cancellationToken);

				// Get the perforce server settings
				PerforceResponse<InfoRecord> infoResponse = await perforce.TryGetInfoAsync(InfoOptions.ShortOutput, cancellationToken);
				if (infoResponse.Succeeded)
				{
					DateTimeOffset? serverDate = infoResponse.Data.ServerDate;
					if (serverDate.HasValue)
					{
						ServerTimeZone = serverDate.Value.Offset;
					}
				}

				// Try to update the zipped binaries list before anything else, because it causes a state change in the UI
				await UpdateArchivesAsync(perforce, cancellationToken);
			}

			while(!cancellationToken.IsCancellationRequested)
			{
				Stopwatch timer = Stopwatch.StartNew();
				Task nextRefreshTask = _refreshEvent.Task;

				using (IPerforceConnection perforce = await PerforceConnection.CreateAsync(_perforceSettings, _logger))
				{
					// Check we still have a valid login ticket
					PerforceResponse<LoginRecord> loginState = await perforce.TryGetLoginStateAsync(cancellationToken);
					if (!loginState.Succeeded)
					{
						LastStatusMessage = "User is not logged in";
						_synchronizationContext.Post(_ => OnLoginExpired?.Invoke(), null);
					}
					else
					{
						// Check we haven't switched streams
						string? newStreamName = await perforce.GetCurrentStreamAsync(cancellationToken);
						if (newStreamName != streamName)
						{
							_synchronizationContext.Post(_ => OnStreamChange?.Invoke(), null);
						}

						// Check for any p4 changes
						if (!await UpdateChangesAsync(perforce, cancellationToken))
						{
							LastStatusMessage = "Failed to update changes";
						}
						else if (!await UpdateChangeTypesAsync(perforce, cancellationToken))
						{
							LastStatusMessage = "Failed to update change types";
						}
						else if (!await UpdateArchivesAsync(perforce, cancellationToken))
						{
							LastStatusMessage = "Failed to update zipped binaries list";
						}
						else
						{
							LastStatusMessage = String.Format("Last update took {0}ms", timer.ElapsedMilliseconds);
						}
					}
				}

				// Wait for another request, or scan for new builds after a timeout
				Task delayTask = Task.Delay(TimeSpan.FromMinutes(IsActive ? 2 : 10), cancellationToken);
				await Task.WhenAny(nextRefreshTask, delayTask);
			}
		}

		async Task<bool> UpdateChangesAsync(IPerforceConnection perforce, CancellationToken cancellationToken)
		{
			// Get the current status of the build
			int maxChanges;
			int oldestChangeNumber = -1;
			int newestChangeNumber = -1;
			HashSet<int> currentChangelists;
			SortedSet<int> prevPromotedChangelists;
			lock(this)
			{
				maxChanges = PendingMaxChanges;
				if(_changes.Count > 0)
				{
					newestChangeNumber = _changes.First().Number;
					oldestChangeNumber = _changes.Last().Number;
				}
				currentChangelists = new HashSet<int>(_changes.Select(x => x.Number));
				prevPromotedChangelists = new SortedSet<int>(_promotedChangeNumbers);
			}

			// Build a full list of all the paths to sync
			List<string> depotPaths = new List<string>();
			if (_selectedClientFileName.EndsWith(".uprojectdirs", StringComparison.InvariantCultureIgnoreCase))
			{
				depotPaths.Add(String.Format("{0}/...", _branchClientPath));
			}
			else
			{
				depotPaths.Add(String.Format("{0}/*", _branchClientPath));
				depotPaths.Add(String.Format("{0}/Engine/...", _branchClientPath));
				depotPaths.Add(String.Format("{0}/...", PerforceUtils.GetClientOrDepotDirectoryName(_selectedClientFileName)));
				if (_isEnterpriseProject)
				{
					depotPaths.Add(String.Format("{0}/Enterprise/...", _branchClientPath));
				}

				// Add in additional paths property
				ConfigSection projectConfigSection = LatestProjectConfigFile.FindSection("Perforce");
				if (projectConfigSection != null)
				{
					IEnumerable<string> additionalPaths = projectConfigSection.GetValues("AdditionalPathsToSync", new string[0]);

					// turn into //ws/path
					depotPaths.AddRange(additionalPaths.Select(p => string.Format("{0}/{1}", _branchClientPath, p.TrimStart('/'))));
				}
			}

			// Read any new changes
			List<ChangesRecord> newChanges;
			if(maxChanges > CurrentMaxChanges)
			{
				newChanges = await perforce.GetChangesAsync(ChangesOptions.IncludeTimes | ChangesOptions.LongOutput, maxChanges, ChangeStatus.Submitted, depotPaths, cancellationToken);
			}
			else
			{
				newChanges = await perforce.GetChangesAsync(ChangesOptions.IncludeTimes | ChangesOptions.LongOutput, -1, ChangeStatus.Submitted, depotPaths.Select(x => $"{x}@>{newestChangeNumber}").ToArray(), cancellationToken);
			}

			// Remove anything we already have
			newChanges.RemoveAll(x => currentChangelists.Contains(x.Number));

			// Update the change ranges
			if(newChanges.Count > 0)
			{
				oldestChangeNumber = Math.Max(oldestChangeNumber, newChanges.Last().Number);
				newestChangeNumber = Math.Min(newestChangeNumber, newChanges.First().Number);
			}

			// If we are using zipped binaries, make sure we have every change since the last zip containing them. This is necessary for ensuring that content changes show as
			// syncable in the workspace view if there have been a large number of content changes since the last code change.
			int minZippedChangeNumber = -1;
			foreach (PerforceArchiveInfo archive in _archives)
			{
				foreach (int changeNumber in archive.ChangeNumberToFileRevision.Keys)
				{
					if (changeNumber > minZippedChangeNumber && changeNumber <= oldestChangeNumber)
					{
						minZippedChangeNumber = changeNumber;
					}
				}
			}
			if(minZippedChangeNumber != -1 && minZippedChangeNumber < oldestChangeNumber)
			{
				string[] filteredPaths = depotPaths.Select(x => $"{x}@{minZippedChangeNumber},{oldestChangeNumber - 1}").ToArray();
				List<ChangesRecord> zipChanges = await perforce.GetChangesAsync(ChangesOptions.None, -1, ChangeStatus.Submitted, filteredPaths, cancellationToken);
				newChanges.AddRange(zipChanges);
			}

			// Fixup any ROBOMERGE authors
			const string roboMergePrefix = "#ROBOMERGE-AUTHOR:";
			foreach (ChangesRecord change in newChanges)
			{
				if(change.Description != null && change.Description.StartsWith(roboMergePrefix))
				{
					int startIdx = roboMergePrefix.Length;
					while(startIdx < change.Description.Length && change.Description[startIdx] == ' ')
					{
						startIdx++;
					}

					int endIdx = startIdx;
					while(endIdx < change.Description.Length && !Char.IsWhiteSpace(change.Description[endIdx]))
					{
						endIdx++;
					}

					if(endIdx > startIdx)
					{
						change.User = change.Description.Substring(startIdx, endIdx - startIdx);
						change.Description = "ROBOMERGE: " + change.Description.Substring(endIdx).TrimStart();
					}
				}
			}

			// Process the new changes received
			if(newChanges.Count > 0 || maxChanges < CurrentMaxChanges)
			{
				// Insert them into the builds list
				lock(this)
				{
					_changes.UnionWith(newChanges);
					if(_changes.Count > maxChanges)
					{
						// Remove changes to shrink it to the max requested size, being careful to avoid removing changes that would affect our ability to correctly
						// show the availability for content changes using zipped binaries.
						SortedSet<ChangesRecord> trimmedChanges = new SortedSet<ChangesRecord>(new PerforceChangeSorter());
						foreach(ChangesRecord change in _changes)
						{
							trimmedChanges.Add(change);
							if(trimmedChanges.Count >= maxChanges && _archives.Any(x => x.ChangeNumberToFileRevision.Count == 0 || x.ChangeNumberToFileRevision.ContainsKey(change.Number) || x.ChangeNumberToFileRevision.First().Key > change.Number))
							{
								break;
							}
						}
						_changes = trimmedChanges;
					}
					CurrentMaxChanges = maxChanges;
				}

				// Find the last submitted change by the current user
				int newLastChangeByCurrentUser = -1;
				foreach(ChangesRecord change in _changes)
				{
					if(String.Compare(change.User, perforce.Settings.UserName, StringComparison.InvariantCultureIgnoreCase) == 0)
					{
						newLastChangeByCurrentUser = Math.Max(newLastChangeByCurrentUser, change.Number);
					}
				}
				LastChangeByCurrentUser = newLastChangeByCurrentUser;

				// Notify the main window that we've got more data
				_synchronizationContext.Post(_ => OnUpdate?.Invoke(), null);
			}
			return true;
		}

		public async Task<bool> UpdateChangeTypesAsync(IPerforceConnection perforce, CancellationToken cancellationToken)
		{
			// Find the changes we need to query
			List<int> queryChangeNumbers = new List<int>();
			lock(this)
			{
				foreach(ChangesRecord change in _changes)
				{
					if(!_changeDetails.ContainsKey(change.Number))
					{
						queryChangeNumbers.Add(change.Number);
					}
				}
			}

			// Update them in batches
			bool updatedConfigFile = false;
			using (CancellationTokenSource cancellationSource = new CancellationTokenSource())
			{
				Task notifyTask = Task.CompletedTask;
				foreach (IReadOnlyList<int> queryChangeNumberBatch in queryChangeNumbers.OrderByDescending(x => x).Batch(10))
				{
					cancellationToken.ThrowIfCancellationRequested();

					// Skip this stuff if the user wants us to query for more changes
					if (PendingMaxChanges != CurrentMaxChanges)
					{
						break;
					}

					// If there's something to check for, find all the content changes after this changelist
					int maxFiles = 100;

					List<DescribeRecord> describeRecords = await perforce.DescribeAsync(DescribeOptions.None, maxFiles, queryChangeNumberBatch.ToArray(), cancellationToken);
					foreach (DescribeRecord describeRecordLoop in describeRecords.OrderByDescending(x => x.Number))
					{
						DescribeRecord describeRecord = describeRecordLoop;
						int queryChangeNumber = describeRecord.Number;

						PerforceChangeDetails details = new PerforceChangeDetails(describeRecord);

						// Content only changes must be flagged accurately, because code changes invalidate precompiled binaries. Increase the number of files fetched until we can classify it correctly.
						while (describeRecord.Files.Count >= maxFiles && !details.ContainsCode)
						{
							cancellationToken.ThrowIfCancellationRequested();
							maxFiles *= 10;

							List<DescribeRecord> newDescribeRecords = await perforce.DescribeAsync(DescribeOptions.None, maxFiles, new int[] { queryChangeNumber }, cancellationToken);
							if (newDescribeRecords.Count == 0)
							{
								break;
							}

							describeRecord = newDescribeRecords[0];
							details = new PerforceChangeDetails(describeRecord);
						}

						lock (this)
						{
							if (!_changeDetails.ContainsKey(queryChangeNumber))
							{
								_changeDetails.Add(queryChangeNumber, details);
							}
						}

						// Reload the config file if it changes
						if (describeRecord.Files.Any(x => x.DepotFile.EndsWith("/UnrealGameSync.ini", StringComparison.OrdinalIgnoreCase)) && !updatedConfigFile)
						{
							await UpdateProjectConfigFileAsync(perforce, cancellationToken);
							updatedConfigFile = true;
						}

						// Notify the caller after a fixed period of time, in case further updates are slow to arrive
						if (notifyTask.IsCompleted)
						{
							notifyTask = Task.Delay(TimeSpan.FromSeconds(5.0), cancellationSource.Token).ContinueWith(_ => _synchronizationContext.Post(_ => OnUpdateMetadata?.Invoke(), null));
						}
					}
				}
				cancellationSource.Cancel();
				await notifyTask.ContinueWith(_ => { }); // Ignore exceptions
			}

			// Find the last submitted code change by the current user
			int newLastCodeChangeByCurrentUser = -1;
			foreach(ChangesRecord change in _changes)
			{
				if(String.Compare(change.User, perforce.Settings.UserName, StringComparison.InvariantCultureIgnoreCase) == 0)
				{
					PerforceChangeDetails? otherDetails;
					if(_changeDetails.TryGetValue(change.Number, out otherDetails) && otherDetails.ContainsCode)
					{
						newLastCodeChangeByCurrentUser = Math.Max(newLastCodeChangeByCurrentUser, change.Number);
					}
				}
			}
			LastCodeChangeByCurrentUser = newLastCodeChangeByCurrentUser;

			// Notify the main window that we've got an update
			_synchronizationContext.Post(_ => OnUpdateMetadata?.Invoke(), null);

			if(_localConfigFiles.Any(x => FileReference.GetLastWriteTimeUtc(x.Key) != x.Value))
			{
				await UpdateProjectConfigFileAsync(perforce, cancellationToken);
				_synchronizationContext.Post(_ => OnUpdateMetadata?.Invoke(), null);
			}

			return true;
		}

		async Task<bool> UpdateArchivesAsync(IPerforceConnection perforce, CancellationToken cancellationToken)
		{
			List<PerforceArchiveInfo> newArchives = await PerforceArchive.EnumerateAsync(perforce, LatestProjectConfigFile, _selectedProjectIdentifier, cancellationToken);

			// Check if the information has changed
			if (!Enumerable.SequenceEqual(_archives, newArchives))
			{
				_archives = newArchives;
				AvailableArchives = _archives.Select(x => (IArchiveInfo)x).ToList();

				if (_changes.Count > 0)
				{
					_synchronizationContext.Post(_ => OnUpdateMetadata?.Invoke(), null);
				}
			}

			return true;
		}

		async Task UpdateProjectConfigFileAsync(IPerforceConnection perforce, CancellationToken cancellationToken)
		{
			_localConfigFiles.Clear();
			LatestProjectConfigFile = await ConfigUtils.ReadProjectConfigFileAsync(perforce, _branchClientPath, _selectedClientFileName, _cacheFolder, _localConfigFiles, _logger, cancellationToken);
		}

		public List<ChangesRecord> GetChanges()
		{
			lock(this)
			{
				return new List<ChangesRecord>(_changes);
			}
		}

		public bool TryGetChangeDetails(int number, [NotNullWhen(true)] out PerforceChangeDetails? details)
		{
			lock(this)
			{
				return _changeDetails.TryGetValue(number, out details);
			}
		}

		public HashSet<int> GetPromotedChangeNumbers()
		{
			lock(this)
			{
				return new HashSet<int>(_promotedChangeNumbers);
			}
		}

		public int LastChangeByCurrentUser
		{
			get;
			private set;
		}

		public int LastCodeChangeByCurrentUser
		{
			get;
			private set;
		}

		public ConfigFile LatestProjectConfigFile
		{
			get;
			private set;
		}

		public IReadOnlyList<IArchiveInfo> AvailableArchives
		{
			get;
			private set;
		}

		public void Refresh()
		{
			_refreshEvent.Set();
		}
	}
}
