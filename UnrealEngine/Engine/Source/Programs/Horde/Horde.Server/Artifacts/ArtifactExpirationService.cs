// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Streams;
using Horde.Server.Server;
using Horde.Server.Storage;
using HordeCommon;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Horde.Server.Artifacts
{
	/// <summary>
	/// Expires artifacts according to their 
	/// </summary>
	class ArtifactExpirationService : IHostedService
	{
		readonly IArtifactCollection _artifactCollection;
		readonly IOptionsMonitor<GlobalConfig> _globalConfig;
		readonly StorageService _storageService;
		readonly IClock _clock;
		readonly ITicker _ticker;
		readonly ILogger _logger;

		public ArtifactExpirationService(IArtifactCollection artifactCollection, StorageService storageService, IOptionsMonitor<GlobalConfig> globalConfig, IClock clock, ILogger<ArtifactExpirationService> logger)
		{
			_artifactCollection = artifactCollection;
			_storageService = storageService;
			_globalConfig = globalConfig;
			_clock = clock;
			_ticker = clock.AddSharedTicker<ArtifactExpirationService>(TimeSpan.FromHours(1.0), TickAsync, logger);
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

		async ValueTask TickAsync(CancellationToken cancellationToken)
		{
			_logger.LogInformation("Checking for expired artifacts...");
			Stopwatch timer = Stopwatch.StartNew();

			GlobalConfig globalConfig = _globalConfig.CurrentValue;

			DateTime utcNow = _clock.UtcNow;
			foreach (ArtifactTypeConfig artifactType in globalConfig.ArtifactTypes)
			{
				DateTime? expireAtUtc = null;
				if (artifactType.KeepDays.HasValue)
				{
					expireAtUtc = utcNow - TimeSpan.FromDays(artifactType.KeepDays.Value);
					_logger.LogInformation("Removing {ArtifactType} artifacts except newer than {Time}", artifactType.Type, expireAtUtc);
				}
				if (artifactType.KeepCount.HasValue)
				{
					_logger.LogInformation("Removing {ArtifactType} artifacts except newest {Count}", artifactType.Type, artifactType.KeepCount.Value);
				}

				Dictionary<StreamId, int> streamIdToCount = new Dictionary<StreamId, int>();
				await foreach (IEnumerable<IArtifact> artifacts in _artifactCollection.FindExpiredAsync(artifactType.Type, expireAtUtc, cancellationToken))
				{
					// Filter the artifacts to keep a maximum count in each stream
					IEnumerable<IArtifact> filteredArtifacts = artifacts;
					if (artifactType.KeepCount != null)
					{
						filteredArtifacts = FilterArtifacts(filteredArtifacts, streamIdToCount, artifactType.KeepCount.Value);
					}

					// Delete the ref allowing the storage service to expire this data
					foreach (IGrouping<NamespaceId, IArtifact> group in filteredArtifacts.GroupBy(x => x.NamespaceId))
					{
						using IStorageClient storageClient = _storageService.CreateClient(group.Key);
						foreach (IArtifact artifact in group)
						{
							_logger.LogInformation("Expiring {StreamId} artifact {ArtifactId}, ref {RefName} (created {CreateTime})", artifact.StreamId, artifact.Id, artifact.RefName, artifact.CreatedAtUtc);
							await storageClient.DeleteRefAsync(artifact.RefName, cancellationToken);
						}
					}

					// Delete the actual artifact objects
					await _artifactCollection.DeleteAsync(filteredArtifacts.Select(x => x.Id), cancellationToken);
				}
			}

			_logger.LogInformation("Finished expiring artifacts in {TimeSecs}s.", (long)timer.Elapsed.TotalSeconds);
		}

		static IEnumerable<IArtifact> FilterArtifacts(IEnumerable<IArtifact> artifacts, Dictionary<StreamId, int> streamIdToCount, int keepCount)
		{
			List<IArtifact> filteredArtifacts = new List<IArtifact>();
			foreach (IArtifact artifact in artifacts)
			{
				int count;
				if (!streamIdToCount.TryGetValue(artifact.StreamId, out count))
				{
					count = 0;
				}

				streamIdToCount[artifact.StreamId] = ++count;
				if (count > keepCount)
				{
					filteredArtifacts.Add(artifact);
				}
			}
			return filteredArtifacts;
		}
	}
}
