// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Horde.Server.Storage;
using HordeCommon;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;

namespace Horde.Server.Artifacts
{
	/// <summary>
	/// Expires artifacts according to their 
	/// </summary>
	class ArtifactExpirationService : IHostedService
	{
		readonly IArtifactCollection _artifactCollection;
		readonly StorageService _storageService;
		readonly IClock _clock;
		readonly ITicker _ticker;
		readonly ILogger _logger;

		public ArtifactExpirationService(IArtifactCollection artifactCollection, StorageService storageService, IClock clock, ILogger<ArtifactExpirationService> logger)
		{
			_artifactCollection = artifactCollection;
			_storageService = storageService;
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
			DateTime utcNow = _clock.UtcNow;
			await foreach (IEnumerable<IArtifact> artifacts in _artifactCollection.FindExpiredAsync(utcNow, cancellationToken))
			{
				foreach (IGrouping<NamespaceId, IArtifact> group in artifacts.GroupBy(x => x.NamespaceId))
				{
					IStorageClient storageClient = await _storageService.GetClientAsync(group.Key, cancellationToken);
					foreach (IArtifact artifact in group)
					{
						_logger.LogDebug("Expiring artifact {ArtifactId}, ref {RefName}", artifact.Id, artifact.RefName);
						await storageClient.DeleteRefAsync(artifact.RefName, cancellationToken);
					}
				}
				await _artifactCollection.DeleteAsync(artifacts.Select(x => x.Id), cancellationToken);
			}
		}
	}
}
