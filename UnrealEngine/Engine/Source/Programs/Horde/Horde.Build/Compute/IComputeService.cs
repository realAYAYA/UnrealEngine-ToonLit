// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Compute;
using EpicGames.Horde.Storage;
using EpicGames.Serialization;
using Horde.Build.Agents.Pools;

namespace Horde.Build.Compute;

/// <summary>
/// Interface for running and scheduling compute tasks (primarily for remote execution)
/// </summary>
public interface IComputeService
{
	/// <summary>
	/// Post tasks to be executed to a channel
	/// </summary>
	/// <param name="clusterId">Cluster to use for execution</param>
	/// <param name="channelId">Unique identifier of the client</param>
	/// <param name="taskRefIds">List of task hashes</param>
	/// <param name="requirementsHash">Requirements document for execution</param>
	/// <returns>Async task</returns>
	Task AddTasksAsync(ClusterId clusterId, ChannelId channelId, List<RefId> taskRefIds, CbObjectAttachment requirementsHash);

	/// <summary>
	/// Gets information about a compute cluster
	/// </summary>
	/// <param name="clusterId">Cluster to use for execution</param>
	Task<IComputeClusterInfo> GetClusterInfoAsync(ClusterId clusterId);

	/// <summary>
	/// Dequeue completed items from a queue and return immediately
	/// </summary>
	/// <param name="clusterId">Cluster containing the channel</param>
	/// <param name="channelId">Queue to remove items from</param>
	/// <returns>List of status updates</returns>
	Task<List<ComputeTaskStatus>> GetTaskUpdatesAsync(ClusterId clusterId, ChannelId channelId);

	/// <summary>
	/// Get number of queued tasks that are compatible for a given pool
	/// </summary>
	/// <param name="clusterId">Cluster to query</param>
	/// <param name="pool">Pool to check for compatibility</param>
	/// <param name="cancellationToken">Cancellation token for the operation</param> 
	/// <returns>Number of tasks in queue</returns>
	Task<int> GetNumQueuedTasksForPoolAsync(ClusterId clusterId, IPool pool, CancellationToken cancellationToken = default);

	/// <summary>
	/// Dequeue completed items from a queue
	/// </summary>
	/// <param name="clusterId">Cluster containing the channel</param>
	/// <param name="channelId">Queue to remove items from</param>
	/// <param name="cancellationToken">Cancellation token to stop waiting for items</param>
	/// <returns>List of status updates</returns>
	Task<List<ComputeTaskStatus>> WaitForTaskUpdatesAsync(ClusterId clusterId, ChannelId channelId, CancellationToken cancellationToken);
}