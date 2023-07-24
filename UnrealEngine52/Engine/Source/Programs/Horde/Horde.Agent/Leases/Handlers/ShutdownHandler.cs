// Copyright Epic Games, Inc. All Rights Reserved.

using System.Threading;
using System.Threading.Tasks;
using Horde.Agent.Services;
using HordeCommon.Rpc.Tasks;
using Microsoft.Extensions.Logging;

namespace Horde.Agent.Leases.Handlers
{
	class ShutdownHandler : LeaseHandler<ShutdownTask>
	{
		readonly ILogger _logger;

		public ShutdownHandler(ILogger<ShutdownHandler> logger)
		{
			_logger = logger;
		}

		/// <inheritdoc/>
		public override Task<LeaseResult> ExecuteAsync(ISession session, string leaseId, ShutdownTask task, CancellationToken cancellationToken)
		{
			_logger.LogInformation("Setting shutdown flag");
			return Task.FromResult(new LeaseResult(SessionResult.Shutdown));
		}
	}
}

