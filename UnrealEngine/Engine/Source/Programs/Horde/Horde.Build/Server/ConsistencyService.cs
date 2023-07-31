// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Horde.Build.Agents.Sessions;
using Horde.Build.Agents.Leases;
using Horde.Build.Utilities;
using HordeCommon;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;

namespace Horde.Build.Server
{
	using SessionId = ObjectId<ISession>;

	/// <summary>
	/// Service which checks the database for consistency and fixes up any errors
	/// </summary>
	class ConsistencyService : IHostedService, IDisposable
	{
		readonly ISessionCollection _sessionCollection;
		readonly ILeaseCollection _leaseCollection;
		readonly ITicker _ticker;
		readonly ILogger<ConsistencyService> _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public ConsistencyService(ISessionCollection sessionCollection, ILeaseCollection leaseCollection, IClock clock, ILogger<ConsistencyService> logger)
		{
			_sessionCollection = sessionCollection;
			_leaseCollection = leaseCollection;
			_ticker = clock.AddSharedTicker<ConsistencyService>(TimeSpan.FromMinutes(20.0), TickLeaderAsync, logger);
			_logger = logger;
		}

		/// <inheritdoc/>
		public Task StartAsync(CancellationToken cancellationToken) => _ticker.StartAsync();

		/// <inheritdoc/>
		public Task StopAsync(CancellationToken cancellationToken) => _ticker.StopAsync();

		/// <inheritdoc/>
		public void Dispose() => _ticker.Dispose();

		/// <summary>
		/// Poll for inconsistencies in the database
		/// </summary>
		/// <param name="stoppingToken">Stopping token</param>
		/// <returns>Async task</returns>
		async ValueTask TickLeaderAsync(CancellationToken stoppingToken)
		{
			List<ISession> sessions = await _sessionCollection.FindActiveSessionsAsync();
			Dictionary<SessionId, ISession> sessionIdToInstance = sessions.ToDictionary(x => x.Id, x => x);

			// Find any leases that are still running when their session has terminated
			List<ILease> leases = await _leaseCollection.FindActiveLeasesAsync();
			foreach (ILease lease in leases)
			{
				if (!sessionIdToInstance.ContainsKey(lease.SessionId))
				{
					ISession? session = await _sessionCollection.GetAsync(lease.SessionId);
					DateTime finishTime = session?.FinishTime ?? DateTime.UtcNow;
					_logger.LogWarning("Setting finish time for lease {LeaseId} to {FinishTime}", lease.Id, finishTime);
					await _leaseCollection.TrySetOutcomeAsync(lease.Id, finishTime, LeaseOutcome.Cancelled, null);
				}
			}
		}
	}
}
