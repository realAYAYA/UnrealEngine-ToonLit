// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Serialization;
using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;

namespace EpicGames.Horde.Compute
{
	/// <summary>
	/// Response decribing a cluster
	/// </summary>
	public interface IComputeClusterInfo
	{
		/// <summary>
		/// Id of the cluster
		/// </summary>
		public ClusterId Id { get; }

		/// <summary>
		/// Namespace containing the requests and responses
		/// </summary>
		public NamespaceId NamespaceId { get; }

		/// <summary>
		/// The request bucket id
		/// </summary>
		public BucketId RequestBucketId { get; }

		/// <summary>
		/// Bucket to store response refs
		/// </summary>
		public BucketId ResponseBucketId { get; }
	}

	/// <summary>
	/// State of a compute task
	/// </summary>
	public enum ComputeTaskState
	{
		/// <summary>
		/// The task is queued for execution
		/// </summary>
		Queued = 0,

		/// <summary>
		/// Currently being executed by a remote agent
		/// </summary>
		Executing = 1,

		/// <summary>
		/// Completed
		/// </summary>
		Complete = 2,
	}

	/// <summary>
	/// Outcome of a task, if it's in the complete state
	/// </summary>
	public enum ComputeTaskOutcome
	{
		/// <summary>
		/// The task was executed successfully
		/// </summary>
		Success = 0,

		/// <summary>
		/// The lease failed with an error
		/// </summary>
		Failed = 1,

		/// <summary>
		/// The lease was cancelled
		/// </summary>
		Cancelled = 2,

		/// <summary>
		/// The task ran but did not return a result for unknown reasons
		/// </summary>
		NoResult = 3,

		/// <summary>
		/// The item was not scheduled for execution in the default time period, and was expired.
		/// </summary>
		Expired = 4,

		/// <summary>
		/// A blob could not be found. The detail field includes the hash of the blob.
		/// </summary>
		BlobNotFound = 5,

		/// <summary>
		/// An uncaught exception occurred during task execution
		/// </summary>
		Exception = 6,
	}

	/// <summary>
	/// Supplies information about the current execution state of a task
	/// </summary>
	public class ComputeTaskStatus
	{
		/// <summary>
		/// Reference to the task decriptor that was requested
		/// </summary>
		[CbField("h")]
		public RefId TaskRefId { get; set; }

		/// <summary>
		/// Time that the event happened
		/// </summary>
		[CbField("t")]
		public DateTime Time { get; set; }

		/// <summary>
		/// New state of the task
		/// </summary>
		[CbField("s")]
		public ComputeTaskState State { get; set; }

		/// <summary>
		/// Outcome of the task execution.Note that this reflects the outcome of the execution rather than the outcome of the task.        
		/// </summary>
		[CbField("o")]
		public ComputeTaskOutcome Outcome { get; set; }

		/// <summary>
		/// Additional information about the outcome of the task.Dependent on the value of Outcome.        
		/// </summary>
		[CbField("d")]
		public string? Detail { get; set; }

		/// <summary>
		/// Ref containing the task result
		/// </summary>
		[CbField("r")]
		public RefId? ResultRefId { get; set; }

		/// <summary>
		/// When transitioning to the executing state, includes the name of the agent performing the work
		/// </summary>
		[CbField("a")]
		public string? AgentId { get; set; }

		/// <summary>
		/// When transitioning to the executing state, includes the id of the lease assigned to the agent
		/// </summary>
		[CbField("l")]
		public string? LeaseId { get; set; }

		/// <summary>
		/// Stats about how the task was queued
		/// </summary>
		[CbField("qs")]
		public ComputeTaskQueueStats? QueueStats { get; set; }

		/// <summary>
		/// Stats for execution of this job
		/// </summary>
		[CbField("es")]
		public ComputeTaskExecutionStats? ExecutionStats { get; set; }
	}

	/// <summary>
	/// Stats for queueing a task
	/// </summary>
	public class ComputeTaskQueueStats
	{
		/// <summary>
		/// Names of scopes 
		/// </summary>
		public static readonly string[] ScopeNames =
		{
			"dispatched",
			"complete"
		};

		private readonly int[] _scopes = new int[ScopeNames.Length];

		/// <summary>
		/// Time that the task was queued
		/// </summary>
		[CbField("t")]
		public DateTime StartTime { get; set; }

		/// <summary>
		/// Timing values measured sequentially starting from StartTime, in milliseconds.
		/// If the order or meaning of fields in this array are changed, the field name should be changed.
		/// </summary>
		[CbField("s")]
#pragma warning disable CA1819 // Properties should not return arrays
		public int[] Scopes
#pragma warning restore CA1819 // Properties should not return arrays
		{
			get => _scopes;
			set => value.AsSpan(0, Math.Max(_scopes.Length, value.Length)).CopyTo(_scopes);
		}

		/// <summary>
		/// Time taken until the task was dequeued
		/// </summary>
		public int QueuedMs
		{
			get => _scopes[0];
			set => _scopes[0] = value;
		}

		/// <summary>
		/// Time that the task was executing
		/// </summary>
		public int ExecutingMs
		{
			get => _scopes[1];
			set => _scopes[1] = value;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public ComputeTaskQueueStats()
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public ComputeTaskQueueStats(DateTime startTime, int queuedMs, int executingMs)
		{
			StartTime = startTime;
			QueuedMs = queuedMs;
			ExecutingMs = executingMs;
		}
	}

	/// <summary>
	/// Stats for executing a task
	/// </summary>
	public class ComputeTaskExecutionStats
	{
		/// <summary>
		/// Names of scopes 
		/// </summary>
		public static readonly string[] ScopeNames =
		{
			"download-ref",
			"download-input",
			"execute",
			"upload-log",
			"upload-output",
			"upload-ref",
		};

		private readonly int[] _scopes = new int[ScopeNames.Length];

		/// <summary>
		/// Start time for execution
		/// </summary>
		[CbField("t")]
		public DateTime StartTime { get; set; }

		/// <summary>
		/// Timing values measured sequentially starting from StartTime, in milliseconds. If the order or meaning of fields in this array are changed, the field name should be changed.
		/// </summary>
		[CbField("s")]
#pragma warning disable CA1819 // Properties should not return arrays
		public int[] Scopes
#pragma warning restore CA1819 // Properties should not return arrays
		{
			get => _scopes;
			set => value.AsSpan(0, Math.Max(_scopes.Length, value.Length)).CopyTo(_scopes);
		}

		/// <summary>
		/// Time taken to download the job specification
		/// </summary>
		public int DownloadRefMs
		{
			get => _scopes[0];
			set => _scopes[0] = value;
		}

		/// <summary>
		/// Time taken to download and unpack the sandbox
		/// </summary>
		public int DownloadInputMs
		{
			get => _scopes[1];
			set => _scopes[1] = value;
		}

		/// <summary>
		/// Time taken to execute the task
		/// </summary>
		public int ExecMs
		{
			get => _scopes[2];
			set => _scopes[2] = value;
		}

		/// <summary>
		/// Time taken to upload log data
		/// </summary>
		public int UploadLogMs
		{
			get => _scopes[3];
			set => _scopes[3] = value;
		}

		/// <summary>
		/// Time taken to upload output data
		/// </summary>
		public int UploadOutputMs
		{
			get => _scopes[4];
			set => _scopes[4] = value;
		}

		/// <summary>
		/// Time taken to upload the output ref
		/// </summary>
		public int UploadRefMs
		{
			get => _scopes[5];
			set => _scopes[5] = value;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public ComputeTaskExecutionStats()
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public ComputeTaskExecutionStats(DateTime startTime, int downloadRefMs, int downloadInputMs, int execMs, int uploadLogMs, int uploadOutputMs, int uploadRefMs)
		{
			StartTime = startTime;

			DownloadRefMs = downloadRefMs;
			DownloadInputMs = downloadInputMs;
			ExecMs = execMs;
			UploadLogMs = uploadLogMs;
			UploadOutputMs = uploadOutputMs;
			UploadRefMs = uploadRefMs;
		}
	}

	/// <summary>
	/// Interface for communicating with a compute server
	/// </summary>
	public interface IComputeClient
	{
		/// <summary>
		/// Gets information about a cluster
		/// </summary>
		/// <param name="clusterId">The cluster to retreive information on</param>
		/// <param name="cancellationToken">Cancellation token for the request</param>
		/// <returns>Cluster information</returns>
		Task<IComputeClusterInfo> GetClusterInfoAsync(ClusterId clusterId, CancellationToken cancellationToken = default);

		/// <summary>
		/// Queues a set of tasks for execution
		/// </summary>
		/// <param name="clusterId">Cluster for executing the tasks</param>
		/// <param name="channelId">Indicates an identifier generated by the client to query responses on</param>
		/// <param name="requirementsHash">Requirements of the task to execute</param>
		/// <param name="taskRefIds">Refs describing the request</param>
		/// <param name="skipCacheLookup">Whether to skip the lookup of cached output for this task</param>
		/// <param name="cancellationToken">Cancellation token for the request</param>
		/// <returns>The new channel id</returns>
		Task AddTasksAsync(ClusterId clusterId, ChannelId channelId, IEnumerable<RefId> taskRefIds, IoHash requirementsHash, bool skipCacheLookup, CancellationToken cancellationToken = default);

		/// <summary>
		/// Read updates from the given remote execution channel
		/// </summary>
		/// <param name="clusterId">Cluster for executing the tasks</param>
		/// <param name="channelId">Channel to receive updates on</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		IAsyncEnumerable<ComputeTaskStatus> GetTaskUpdatesAsync(ClusterId clusterId, ChannelId channelId, CancellationToken cancellationToken = default);
	}

	/// <summary>
	/// Extension methods for <see cref="IComputeClient"/>
	/// </summary>
	public static class ComputeClientExtensions
	{
		/// <summary>
		/// Queues a single task for execution
		/// </summary>
		/// <param name="computeClient">The compute client instance</param>
		/// <param name="clusterId">Name of the profile to use for executing the tasks</param>
		/// <param name="channelId">Channel to receive updates on</param>
		/// <param name="taskRefId">Ref describing the request</param>
		/// <param name="requirementsHash">Requirements of the task to execute</param>
		/// <param name="skipCacheLookup">Whether to skip the lookup of cached output for this task</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public static Task AddTaskAsync(this IComputeClient computeClient, ClusterId clusterId, ChannelId channelId, RefId taskRefId, IoHash requirementsHash, bool skipCacheLookup, CancellationToken cancellationToken = default)
		{
			return computeClient.AddTasksAsync(clusterId, channelId, new[] { taskRefId }, requirementsHash, skipCacheLookup, cancellationToken);
		}
	}
}
