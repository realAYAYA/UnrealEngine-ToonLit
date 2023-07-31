// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using Horde.Build.Agents.Pools;

namespace Horde.Build.Agents.Fleet
{
	/// <summary>
	/// Service to manage a fleet of machines
	/// </summary>
	public interface IFleetManager
	{
		/// <summary>
		/// Expand the given pool
		/// </summary>
		/// <param name="pool">Pool to resize</param>
		/// <param name="agents">Current list of agents in the pool</param>
		/// <param name="count">Number of agents to add</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>Async task</returns>
		Task ExpandPoolAsync(IPool pool, IReadOnlyList<IAgent> agents, int count, CancellationToken cancellationToken = default);

		/// <summary>
		/// Shrink the given pool
		/// </summary>
		/// <param name="pool">Pool to resize</param>
		/// <param name="agents">Current list of agents in the pool</param>
		/// <param name="count">Number of agents to remove</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>Async task</returns>
		Task ShrinkPoolAsync(IPool pool, IReadOnlyList<IAgent> agents, int count, CancellationToken cancellationToken = default);

		/// <summary>
		/// Returns the number of stopped instances in the given pool
		/// </summary>
		/// <param name="pool">Pool to resize</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>Async task</returns>
		Task<int> GetNumStoppedInstancesAsync(IPool pool, CancellationToken cancellationToken = default);
	}
}

