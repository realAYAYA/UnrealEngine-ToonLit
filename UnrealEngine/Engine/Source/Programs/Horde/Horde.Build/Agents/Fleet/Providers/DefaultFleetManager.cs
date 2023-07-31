// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using Horde.Build.Agents.Pools;
using Microsoft.Extensions.Logging;

namespace Horde.Build.Agents.Fleet.Providers
{
	/// <summary>
	/// Default implementation of <see cref="IFleetManager"/>
	/// </summary>
	public class DefaultFleetManager : IFleetManager
	{
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="logger">Logging device</param>
		public DefaultFleetManager(ILogger<DefaultFleetManager> logger)
		{
			_logger = logger;
		}

		/// <inheritdoc/>
		public Task ExpandPoolAsync(IPool pool, IReadOnlyList<IAgent> agents, int count, CancellationToken cancellationToken)
		{
			_logger.LogInformation("Expand pool {PoolId} by {Count} agents", pool.Id, count);
			return Task.CompletedTask;
		}

		/// <inheritdoc/>
		public Task ShrinkPoolAsync(IPool pool, IReadOnlyList<IAgent> agents, int count, CancellationToken cancellationToken)
		{
			_logger.LogInformation("Shrink pool {PoolId} by {Count} agents", pool.Id, count);
			return Task.CompletedTask;
		}

		/// <inheritdoc/>
		public Task<int> GetNumStoppedInstancesAsync(IPool pool, CancellationToken cancellationToken) => Task.FromResult(0);
	}
}
