// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Nodes;
using Horde.Server.Server;
using Horde.Server.Storage;
using Horde.Server.Streams;
using Horde.Server.Utilities;
using HordeCommon;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Horde.Server.Perforce
{
	/// <summary>
	/// Exception triggered during content replication
	/// </summary>
	public sealed class ReplicationException : Exception
	{
		internal ReplicationException(string message) : base(message)
		{
		}
	}

	/// <summary>
	/// Service which replicates content from Perforce
	/// </summary>
	sealed class ReplicationService : IHostedService
	{
		readonly IStreamCollection _streamCollection;
		readonly IPerforceService _perforceService;
		readonly PerforceReplicator _replicator;
		readonly StorageService _storageService;
		readonly IMemoryCache _memoryCache;
		readonly ITicker _ticker;
		readonly IOptionsMonitor<GlobalConfig> _globalConfig;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public ReplicationService(IStreamCollection streamCollection, IPerforceService perforceService, PerforceReplicator replicator, StorageService storageService, IMemoryCache memoryCache, IClock clock, IOptionsMonitor<GlobalConfig> globalConfig, ILogger<ReplicationService> logger)
		{
			_streamCollection = streamCollection;
			_perforceService = perforceService;
			_replicator = replicator;
			_storageService = storageService;
			_memoryCache = memoryCache;
			_ticker = clock.AddSharedTicker<ReplicationService>(TimeSpan.FromSeconds(20.0), TickSharedAsync, logger);
			_globalConfig = globalConfig;
			_logger = logger;
		}

		/// <inheritdoc/>
		public async Task StartAsync(CancellationToken cancellationToken)
		{
			await _ticker.StartAsync();
		}

		/// <inheritdoc/>
		public async Task StopAsync(CancellationToken cancellationToken)
		{
			await _ticker.StopAsync();
		}

		async ValueTask TickSharedAsync(CancellationToken cancellationToken)
		{
			Dictionary<StreamId, BackgroundTask> streamIdToTask = new Dictionary<StreamId, BackgroundTask>();
			try
			{
				for (; ; )
				{
					IReadOnlyList<StreamConfig> streamConfigs = _globalConfig.CurrentValue.Streams;

					HashSet<StreamId> removeStreams = new HashSet<StreamId>(streamIdToTask.Keys);
					foreach (StreamConfig streamConfig in streamConfigs)
					{
						if (streamConfig.ReplicationMode == ContentReplicationMode.Full)
						{
							removeStreams.Remove(streamConfig.Id);
							if (!streamIdToTask.ContainsKey(streamConfig.Id))
							{
								streamIdToTask.Add(streamConfig.Id, BackgroundTask.StartNew(ctx => RunReplicationGuardedAsync(streamConfig, ctx)));
								_logger.LogInformation("Started replication of {StreamId}", streamConfig.Id);
							}
						}
					}

					foreach (StreamId removeStreamId in removeStreams)
					{
						if (streamIdToTask.Remove(removeStreamId, out BackgroundTask? task))
						{
							await task.DisposeAsync();
							_logger.LogInformation("Stopped replication of {StreamId}", removeStreamId);
						}
					}

					await Task.Delay(TimeSpan.FromMinutes(1.0), cancellationToken);
				}
			}
			finally
			{
				await Parallel.ForEachAsync(streamIdToTask.Values, (x, ctx) => x.DisposeAsync());
			}
		}

		async Task RunReplicationGuardedAsync(StreamConfig streamConfig, CancellationToken cancellationToken)
		{
			for (; ; )
			{
				try
				{
					await RunReplicationAsync(streamConfig, cancellationToken);
				}
				catch (OperationCanceledException) when (cancellationToken.IsCancellationRequested)
				{
					break;
				}
				catch (Exception ex)
				{
					_logger.LogError(ex, "Exception running replication for {StreamId}: {Message}", streamConfig.Id, ex.Message);
				}

				await Task.Delay(TimeSpan.FromSeconds(20.0), cancellationToken);
			}
		}

		async Task RunReplicationAsync(StreamConfig streamConfig, CancellationToken cancellationToken)
		{
			RefName refName = new RefName(streamConfig.Id.ToString());

			IStorageClientImpl store = await _storageService.GetClientAsync(Namespace.Perforce, cancellationToken);

			CommitNode? lastCommitNode = await store.TryReadNodeAsync<CommitNode>(refName, cancellationToken: cancellationToken);
			ICommitCollection commits = _perforceService.GetCommits(streamConfig);

			PerforceReplicationOptions options = new PerforceReplicationOptions();

			ICommit commit;
			if (lastCommitNode == null)
			{
				commit = await commits.GetLatestAsync(cancellationToken);
			}
			else
			{
				commit = await commits.SubscribeAsync(lastCommitNode.Number, cancellationToken: cancellationToken).FirstAsync(cancellationToken);
			}

			for (; ; )
			{
				_logger.LogInformation("Replicating {StreamId} change {Change}", streamConfig.Id, commit.Number);
				await _replicator.WriteAsync(streamConfig, commit.Number, options, cancellationToken);
				commit = await commits.SubscribeAsync(commit.Number, cancellationToken: cancellationToken).FirstAsync(cancellationToken);
			}
		}
	}
}
