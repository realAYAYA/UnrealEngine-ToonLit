// Copyright Epic Games, Inc. All Rights Reserved.

using System.Threading;
using System.Threading.Tasks;
using Horde.Agent.Services;
using Horde.Agent.Utility;
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
			_logger.LogInformation("Scheduling shutdown task");
			SessionResult result = new SessionResult((logger, ctx) => Shutdown.ExecuteAsync(false, logger, ctx));
			return Task.FromResult(new LeaseResult(result));
		}
	}
}

