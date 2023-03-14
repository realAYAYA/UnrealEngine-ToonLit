// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Bundles;
using EpicGames.Horde.Storage.Nodes;
using EpicGames.Perforce;
using EpicGames.Redis;
using EpicGames.Serialization;
using Horde.Build.Server;
using Horde.Build.Streams;
using Horde.Build.Users;
using Horde.Build.Utilities;
using HordeCommon;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using StackExchange.Redis;

namespace Horde.Build.Perforce
{
	using CommitId = ObjectId<ICommit>;
	using StreamId = StringId<IStream>;

	/// <summary>
	/// Options for the commit service
	/// </summary>
	public class CommitServiceOptions
	{
		/// <summary>
		/// Whether to mirror commit metadata to the database
		/// </summary>
		public bool Enable { get; set; } = true;
	}

	/// <summary>
	/// Service which mirrors changes from Perforce
	/// </summary>
	class CommitService : ICommitService, IHostedService, IDisposable
	{
		/// <summary>
		/// Metadata about a stream that needs to have commits mirrored
		/// </summary>
		class StreamInfo
		{
			public IStream Stream { get; set; }
			public ViewMap View { get; }

			public StreamInfo(IStream stream, ViewMap view)
			{
				Stream = stream;
				View = view;
			}
		}

		/// <summary>
		/// A registered listener for new commits
		/// </summary>
		sealed class ListenerInfo : IAsyncDisposable
		{
			readonly CommitService _owner;
			public Func<ICommit, Task> Callback { get; }

			public ListenerInfo(CommitService owner, Func<ICommit, Task> callback)
			{
				_owner = owner;
				Callback = callback;
			}

			public async ValueTask DisposeAsync() => await _owner.RemoveListenerAsync(this);
		}

		public CommitServiceOptions Options { get; }

		// Redis
		readonly RedisConnectionPool _redisConnectionPool;

		// Collections
		readonly ICommitCollection _commitCollection;
		readonly IStreamCollection _streamCollection;
		readonly IPerforceService _perforceService;
		readonly IUserCollection _userCollection;
		readonly ILogger<CommitService> _logger;

		// Listeners
		readonly object _lockObject = new object();
		Task _listenerTask = Task.CompletedTask;
		readonly List<ListenerInfo> _listeners = new List<ListenerInfo>();

		// Metadata
		readonly ITicker _updateMetadataTicker;

		static RedisKey LastChangeKey(StreamId streamId) => $"commits/metadata/{streamId}";

		/// <summary>
		/// Constructor
		/// </summary>
		public CommitService(RedisService redisService, ICommitCollection commitCollection, IStreamCollection streamCollection, IPerforceService perforceService, IUserCollection userCollection, IClock clock, IOptions<CommitServiceOptions> options, ILogger<CommitService> logger)
		{
			Options = options.Value;

			_redisConnectionPool = redisService.ConnectionPool;
			_commitCollection = commitCollection;
			_streamCollection = streamCollection;
			_perforceService = perforceService;
			_userCollection = userCollection;
			_logger = logger;
			_updateMetadataTicker = clock.AddSharedTicker<CommitService>(TimeSpan.FromSeconds(30.0), UpdateMetadataAsync, logger);
		}

		/// <inheritdoc/>
		public void Dispose() => _updateMetadataTicker.Dispose();

		/// <inheritdoc/>
		public async Task StartAsync(CancellationToken cancellationToken)
		{
			if (Options.Enable)
			{
				await _updateMetadataTicker.StartAsync();
			}
		}

		/// <inheritdoc/>
		public async Task StopAsync(CancellationToken cancellationToken)
		{
			if (Options.Enable)
			{
				await _updateMetadataTicker.StopAsync();
			}
		}

		#region Notifications

		public IAsyncDisposable AddListener(Func<ICommit, Task> onAddCommit)
		{
			ListenerInfo listener = new ListenerInfo(this, onAddCommit);
			lock (_lockObject)
			{
				_listeners.Add(listener);
			}
			return listener;
		}

		Task RemoveListenerAsync(ListenerInfo listener)
		{
			Task waitTask;
			lock (_lockObject)
			{
				waitTask = _listenerTask.ContinueWith(x => RemoveListener(listener), TaskScheduler.Default);
				_listenerTask = waitTask;
			}
			return waitTask;
		}

		void RemoveListener(ListenerInfo listener)
		{
			lock (_lockObject)
			{
				_listeners.Remove(listener);
			}
		}

		private Task NotifyListenersAsync(ICommit commit)
		{
			Task waitTask;
			lock (_lockObject)
			{
				waitTask = _listenerTask.ContinueWith(x => NotifyAllListenersAsync(commit), TaskScheduler.Default);
				_listenerTask = waitTask;
			}
			return waitTask;
		}

		async Task NotifyAllListenersAsync(ICommit commit)
		{
			List<ListenerInfo> listeners;
			lock (_lockObject)
			{
				listeners = new List<ListenerInfo>(_listeners);
			}
			await Task.WhenAll(listeners.Select(x => x.Callback(commit)));
		}

		#endregion

		#region Metadata updates

		/// <summary>
		/// Polls Perforce for submitted changes
		/// </summary>
		/// <param name="cancellationToken"></param>
		/// <returns></returns>
		async ValueTask UpdateMetadataAsync(CancellationToken cancellationToken)
		{
			List<IStream> streams = await _streamCollection.FindAllAsync();
			foreach (IGrouping<string, IStream> group in streams.GroupBy(x => x.ClusterName, StringComparer.OrdinalIgnoreCase))
			{
				try
				{
					await UpdateMetadataForClusterAsync(group.Key, group);
				}
				catch (Exception ex)
				{
					_logger.LogError(ex, "Error while updating cluster {ClusterName}", group.Key);
				}
			}
		}

		/// <summary>
		/// Updates metadata for all the streams on a particular server cluster
		/// </summary>
		/// <param name="clusterName"></param>
		/// <param name="streams"></param>
		/// <returns></returns>
		async Task UpdateMetadataForClusterAsync(string clusterName, IEnumerable<IStream> streams)
		{
			// Find the minimum changelist number to query
			Dictionary<IStream, int> streamToNextChange = new Dictionary<IStream, int>();
			foreach (IStream stream in streams)
			{
				RedisValue lastChange = await _redisConnectionPool.GetDatabase().StringGetAsync(LastChangeKey(stream.Id));

				int nextChange = 0;
				if (lastChange.HasValue && lastChange.IsInteger)
				{
					nextChange = (int)lastChange + 1;
				}
				
				streamToNextChange[stream] = nextChange;
			}

			// Update the database with all the commits
			HashSet<StreamId> newStreamIds = new HashSet<StreamId>();
			await foreach (NewCommit newCommit in FindCommitsForClusterAsync(clusterName, streamToNextChange))
			{
				ICommit commit = await _commitCollection.AddOrReplaceAsync(newCommit);
				_logger.LogInformation("Replicated CL {Change}: {Description} as {CommitId}", commit.Change, StringUtils.Truncate(commit.Description, 40), commit.Id);

				await NotifyListenersAsync(commit);

				newStreamIds.Add(commit.StreamId);
			}
		}

		/// <summary>
		/// Enumerate new commits to the given streams, using a stream view to deduplicate changes which affect multiple branches.
		/// </summary>
		/// <param name="clusterName"></param>
		/// <param name="streamToFirstChange"></param>
		/// <returns>List of new commits</returns>
		public async IAsyncEnumerable<NewCommit> FindCommitsForClusterAsync(string clusterName, Dictionary<IStream, int> streamToFirstChange)
		{
			_logger.LogInformation("Replicating metadata for cluster {Name}", clusterName);

			// Create a connection to the server
			using IPerforceConnection? connection = await _perforceService.GetServiceUserConnection(clusterName);
			if (connection == null)
			{
				throw new PerforceException($"Unable to create cluster connection for {clusterName}");
			}

			// Figure out the case settings for the server
			InfoRecord serverInfo = await connection.GetInfoAsync(InfoOptions.ShortOutput);

			// Get the view for each stream
			List<StreamInfo> streamInfoList = new List<StreamInfo>();
			foreach (IStream stream in streamToFirstChange.Keys)
			{
				ViewMap view = await GetStreamViewAsync(connection, stream.Name);
				streamInfoList.Add(new StreamInfo(stream, view));
			}

			// Find all the depot roots
			Dictionary<Utf8String, Utf8String> depotRoots = new Dictionary<Utf8String, Utf8String>(serverInfo.Utf8PathComparer);
			foreach (ViewMap view in streamInfoList.Select(x => x.View))
			{
				foreach (ViewMapEntry entry in view.Entries)
				{
					if (entry.SourcePrefix.Length >= 3)
					{
						int slashIdx = entry.SourcePrefix.IndexOf('/', 2);
						Utf8String depot = entry.SourcePrefix.Slice(0, slashIdx + 1);

						Utf8String depotRoot;
						if (depotRoots.TryGetValue(depot, out depotRoot))
						{
							depotRoot = GetCommonPrefix(depotRoot, entry.SourcePrefix);
						}
						else
						{
							depotRoot = entry.SourcePrefix;
						}

						int lastSlashIdx = depotRoot.LastIndexOf('/');
						depotRoots[depot] = depotRoot.Slice(0, lastSlashIdx + 1);
					}
				}
			}

			// Find the minimum changelist number to query
			int minChange = Int32.MaxValue;
			foreach (StreamInfo streamInfo in streamInfoList)
			{
				int firstChange = streamToFirstChange[streamInfo.Stream];
				if (firstChange == 0)
				{
					firstChange = await GetFirstCommitToReplicateAsync(connection, streamInfo.View, serverInfo.Utf8PathComparer);
				}
				if (firstChange != 0)
				{
					minChange = Math.Min(minChange, firstChange);
				}
			}

			// Find all the changes to consider
			SortedSet<int> changeNumbers = new SortedSet<int>();
			foreach (Utf8String depotRoot in depotRoots.Values)
			{
				List<ChangesRecord> changes = await connection.GetChangesAsync(ChangesOptions.None, null, minChange, -1, ChangeStatus.Submitted, null, $"{depotRoot}...");
				changeNumbers.UnionWith(changes.Select(x => x.Number));
			}

			// Add the changes in order
			List<string> relativePaths = new List<string>();
			foreach (int changeNumber in changeNumbers)
			{
				DescribeRecord describeRecord = await connection.DescribeAsync(changeNumber);
				foreach (StreamInfo streamInfo in streamInfoList)
				{
					IStream stream = streamInfo.Stream;

					Utf8String basePath = GetBasePath(describeRecord, streamInfo.View, serverInfo.Utf8PathComparer);
					if (!basePath.IsEmpty)
					{
						IUser author = await _userCollection.FindOrAddUserByLoginAsync(describeRecord.User);
						IUser owner = (await ParseRobomergeOwnerAsync(describeRecord.Description)) ?? author;

						int originalChange = ParseRobomergeSource(describeRecord.Description) ?? describeRecord.Number;

						yield return new NewCommit(stream.Id, describeRecord.Number, originalChange, author.Id, owner.Id, describeRecord.Description, basePath.ToString(), describeRecord.Time);
					}
				}
			}
		}

		/// <summary>
		/// Find the first commit to replicate from a branch
		/// </summary>
		/// <param name="connection"></param>
		/// <param name="view"></param>
		/// <param name="comparer"></param>
		/// <returns></returns>
		static async Task<int> GetFirstCommitToReplicateAsync(IPerforceConnection connection, ViewMap view, Utf8StringComparer comparer)
		{
			int minChange = 0;

			List<Utf8String> rootPaths = view.GetRootPaths(comparer);
			foreach (Utf8String rootPath in rootPaths)
			{
				IList<ChangesRecord> pathChanges = await connection.GetChangesAsync(ChangesOptions.None, 20, ChangeStatus.Submitted, $"{rootPath}...");
				minChange = Math.Max(minChange, pathChanges.Min(x => x.Number));
			}

			return minChange;
		}

		/// <summary>
		/// Find the base path for a change within a stream
		/// </summary>
		/// <param name="changelist">Information about the changelist</param>
		/// <param name="view">Mapping from depot syntax to client view</param>
		/// <param name="comparer">Path comparison type for the server</param>
		/// <returns>The base path for all files in the change</returns>
		static Utf8String GetBasePath(DescribeRecord changelist, ViewMap view, Utf8StringComparer comparer)
		{
			Utf8String basePath = default;
			foreach (DescribeFileRecord file in changelist.Files)
			{
				Utf8String streamFile;
				if (view.TryMapFile(file.DepotFile, comparer, out streamFile))
				{
					if (basePath.IsEmpty)
					{
						basePath = streamFile;
					}
					else
					{
						basePath = GetCommonPrefix(basePath, streamFile);
					}
				}
			}
			return basePath;
		}

		/// <summary>
		/// Gets the common prefix between two stringsc
		/// </summary>
		/// <param name="a"></param>
		/// <param name="b"></param>
		/// <returns></returns>
		static Utf8String GetCommonPrefix(Utf8String a, Utf8String b)
		{
			int index = 0;
			while (index < a.Length && index < b.Length && a[index] == b[index])
			{
				index++;
			}
			return a.Substring(0, index);
		}

		/// <inheritdoc/>
		public async Task<List<ICommit>> FindCommitsAsync(StreamId streamId, int? minChange = null, int? maxChange = null, string[]? paths = null, int? index = null, int? count = null)
		{
			if(paths != null)
			{
				throw new NotImplementedException();
			}

			return await _commitCollection.FindCommitsAsync(streamId, minChange, maxChange, index, count);
		}

		/// <summary>
		/// Attempts to parse the Robomerge source from this commit information
		/// </summary>
		/// <param name="description">Description text to parse</param>
		/// <returns>The parsed source changelist, or null if no #ROBOMERGE-SOURCE tag was present</returns>
		static int? ParseRobomergeSource(string description)
		{
			// #ROBOMERGE-SOURCE: CL 13232051 in //Fortnite/Release-12.60/... via CL 13232062 via CL 13242953
			Match match = Regex.Match(description, @"^#ROBOMERGE-SOURCE: CL (\d+)", RegexOptions.Multiline);
			if (match.Success)
			{
				return Int32.Parse(match.Groups[1].Value, CultureInfo.InvariantCulture);
			}
			else
			{
				return null;
			}
		}

		async Task<IUser?> ParseRobomergeOwnerAsync(string description)
		{
			string? originalAuthor = ParseRobomergeOwner(description);
			if (originalAuthor != null)
			{
				return await _userCollection.FindOrAddUserByLoginAsync(originalAuthor);
			}
			return null;
		}

		/// <summary>
		/// Attempts to parse the Robomerge owner from this commit information
		/// </summary>
		/// <param name="description">Description text to parse</param>
		/// <returns>The Robomerge owner, or null if no #ROBOMERGE-OWNER tag was present</returns>
		static string? ParseRobomergeOwner(string description)
		{
			// #ROBOMERGE-OWNER: ben.marsh
			Match match = Regex.Match(description, @"^#ROBOMERGE-OWNER:\s*([^\s]+)", RegexOptions.Multiline);
			if (match.Success)
			{
				return match.Groups[1].Value;
			}
			else
			{
				return null;
			}
		}

		/// <summary>
		/// Gets the view for a stream
		/// </summary>
		/// <param name="connection">The Perforce connection</param>
		/// <param name="streamName">Name of the stream to be mirrored</param>
		static async Task<ViewMap> GetStreamViewAsync(IPerforceConnection connection, string streamName)
		{
			StreamRecord record = await connection.GetStreamAsync(streamName, true);

			ViewMap view = new ViewMap();
			foreach (string viewLine in record.View)
			{
				Match match = Regex.Match(viewLine, "^(-?)([^ ]+) *([^ ]+)$");
				if (!match.Success)
				{
					throw new PerforceException($"Unable to parse stream view: '{viewLine}'");
				}
				view.Entries.Add(new ViewMapEntry(match.Groups[1].Length == 0, match.Groups[2].Value, match.Groups[3].Value));
			}

			return view;
		}

		#endregion
	}
}
