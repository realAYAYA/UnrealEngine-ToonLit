// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Compute;
using EpicGames.Horde.Storage;
using EpicGames.Redis;
using EpicGames.Serialization;
using Google.Protobuf;
using Google.Protobuf.WellKnownTypes;
using Horde.Build.Agents;
using Horde.Build.Agents.Leases;
using Horde.Build.Agents.Pools;
using Horde.Build.Server;
using Horde.Build.Tasks;
using Horde.Build.Utilities;
using HordeCommon;
using HordeCommon.Rpc.Tasks;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using OpenTracing;
using OpenTracing.Util;
using StackExchange.Redis;

namespace Horde.Build.Compute
{
	using LeaseId = ObjectId<ILease>;

	/// <summary>
	/// Information about a particular task
	/// </summary>
	[RedisConverter(typeof(RedisCbConverter<>))]
	class ComputeTaskInfo
	{
		[CbField("c")]
		public ClusterId ClusterId { get; set; }

		[CbField("h")]
		public RefId TaskRefId { get; set; }

		[CbField("ch")]
		public ChannelId ChannelId { get; set; }

		[CbField("q")]
		public DateTime QueuedAt { get; set; }

		private ComputeTaskInfo()
		{
		}

		public ComputeTaskInfo(ClusterId clusterId, RefId taskRefId, ChannelId channelId, DateTime queuedAt)
		{
			ClusterId = clusterId;
			TaskRefId = taskRefId;
			ChannelId = channelId;
			QueuedAt = queuedAt;
		}
	}

	/// <summary>
	/// Dispatches remote actions. Does not implement any cross-pod communication to satisfy leases; only agents connected to this server instance will be stored.
	/// </summary>
	public class ComputeService : TaskSourceBase<ComputeTaskMessage>, IHostedService, IDisposable, IComputeService
	{
		[RedisConverter(typeof(QueueKeySerializer))]
		class QueueKey
		{
			public ClusterId ClusterId { get; set; }
			public IoHash RequirementsHash { get; set; }

			public QueueKey(ClusterId clusterId, IoHash requirementsHash)
			{
				ClusterId = clusterId;
				RequirementsHash = requirementsHash;
			}

			public override string ToString() => $"{ClusterId}/{RequirementsHash}";
		}

		class QueueKeySerializer : IRedisConverter<QueueKey>
		{
			public QueueKey FromRedisValue(RedisValue value)
			{
				string str = value.ToString();
				int idx = str.LastIndexOf("/", StringComparison.Ordinal);
				return new QueueKey(new ClusterId(str.Substring(0, idx)), IoHash.Parse(str.Substring(idx + 1)));
			}

			public RedisValue ToRedisValue(QueueKey value) => $"{value.ClusterId}/{value.RequirementsHash}";
		}

		class ClusterInfo : IComputeClusterInfo
		{
			public ClusterId Id { get; set; }
			public NamespaceId NamespaceId { get; set; }
			public BucketId RequestBucketId { get; set; }
			public BucketId ResponseBucketId { get; set; }

			public ClusterInfo(ComputeClusterConfig config)
			{
				Id = new ClusterId(config.Id);
				NamespaceId = new NamespaceId(config.NamespaceId);
				RequestBucketId = new BucketId(config.RequestBucketId);
				ResponseBucketId = new BucketId(config.ResponseBucketId);
			}
		}

		/// <inheritdoc/>
		public override string Type => "Compute";

		/// <inheritdoc/>
		public override TaskSourceFlags Flags => TaskSourceFlags.None;

		/// <summary>
		/// ID of the default namespace
		/// </summary>
		public static NamespaceId DefaultNamespaceId { get; } = new NamespaceId("default");

		readonly IStorageClient _storageClient;
		readonly ITaskScheduler<QueueKey, ComputeTaskInfo> _taskScheduler;
		readonly RedisMessageQueue<ComputeTaskStatus> _messageQueue;
		readonly ITicker _expireTasksTicker;
		readonly IMemoryCache _requirementsCache;
		readonly LazyCachedValue<Task<Globals>> _globals;
		readonly ILogger _logger;

		static ComputeService()
		{
			RedisSerializer.RegisterConverter<ComputeTaskStatus, RedisCbConverter<ComputeTaskStatus>>();
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public ComputeService(MongoService mongoService, RedisService redisService, IStorageClient storageClient, IClock clock, ILogger<ComputeService> logger)
		{
			_storageClient = storageClient;
			_taskScheduler = new RedisTaskScheduler<QueueKey, ComputeTaskInfo>(redisService.ConnectionPool, "compute/tasks/", logger);
			_messageQueue = new RedisMessageQueue<ComputeTaskStatus>(redisService.GetDatabase(), "compute/messages/");
			_expireTasksTicker = clock.AddTicker<ComputeService>(TimeSpan.FromMinutes(2.0), ExpireTasksAsync, logger);
			_requirementsCache = new MemoryCache(new MemoryCacheOptions());
			_globals = new LazyCachedValue<Task<Globals>>(() => mongoService.GetGlobalsAsync(), TimeSpan.FromSeconds(120.0));
			_logger = logger;

			OnLeaseStartedProperties.Add(x => x.TaskRefId);
		}

		static ComputeTaskStatus CreateStatus(RefId taskRefId, ComputeTaskState state)
		{
			ComputeTaskStatus status = new ComputeTaskStatus();
			status.TaskRefId = taskRefId;
			status.Time = DateTime.UtcNow;
			status.State = state;
			return status;
		}

		/// <inheritdoc/>
		public Task StartAsync(CancellationToken token) => _expireTasksTicker.StartAsync();

		/// <inheritdoc/>
		public Task StopAsync(CancellationToken token) => _expireTasksTicker.StopAsync();

		/// <summary>
		/// Expire tasks that are in inactive queues (ie. no machines can execute them)
		/// </summary>
		/// <param name="cancellationToken"></param>
		/// <returns></returns>
		async ValueTask ExpireTasksAsync(CancellationToken cancellationToken)
		{
			using IScope scope = GlobalTracer.Instance.BuildSpan("ComputeService.ExpireTasksAsync").StartActive();
			List<QueueKey> queueKeys = await _taskScheduler.GetInactiveQueuesAsync();
			scope.Span.SetTag("numQueueKeys", queueKeys.Count);
			
			foreach (QueueKey queueKey in queueKeys)
			{
				_logger.LogInformation("Inactive queue: {QueueKey}", queueKey);
				for (; ; )
				{
					ComputeTaskInfo? computeTask = await _taskScheduler.DequeueAsync(queueKey);
					if (computeTask == null)
					{
						break;
					}

					ComputeTaskStatus status = CreateStatus(computeTask.TaskRefId, ComputeTaskState.Complete);
					status.Outcome = ComputeTaskOutcome.Expired;
					status.Detail = $"No agents monitoring queue {queueKey}";

					_logger.LogInformation("Compute task expired (queue: {RequirementsHash}, task: {TaskHash}, channel: {ChannelId})", queueKey, computeTask.TaskRefId, computeTask.ChannelId);
					await PostStatusMessageAsync(computeTask, status);
				}
			}
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			_messageQueue.Dispose();
			_expireTasksTicker.Dispose();
			_requirementsCache.Dispose();
		}

		/// <inheritdoc/>
		public async Task<IComputeClusterInfo> GetClusterInfoAsync(ClusterId clusterId)
		{
			ComputeClusterConfig? config = await GetClusterAsync(clusterId);
			if (config == null)
			{
				throw new KeyNotFoundException();
			}
			return new ClusterInfo(config);
		}

		/// <inheritdoc/>
		public async Task AddTasksAsync(ClusterId clusterId, ChannelId channelId, List<RefId> taskRefIds, CbObjectAttachment requirementsHash)
		{
			List<Task> tasks = new List<Task>();
			foreach (RefId taskRefId in taskRefIds)
			{
				ComputeTaskInfo taskInfo = new ComputeTaskInfo(clusterId, taskRefId, channelId, DateTime.UtcNow);
				_logger.LogDebug("Adding task {TaskHash} from channel {ChannelId} to queue {ClusterId}{RequirementsHash}", taskRefId.Hash, channelId, clusterId, requirementsHash);
				tasks.Add(_taskScheduler.EnqueueAsync(new QueueKey(clusterId, requirementsHash), taskInfo, false));
			}
			await Task.WhenAll(tasks);
		}

		async ValueTask<ComputeClusterConfig?> GetClusterAsync(ClusterId clusterId)
		{
			Globals globalsInstance = await _globals.GetCached();
			return globalsInstance.ComputeClusters.FirstOrDefault(x => new ClusterId(x.Id) == clusterId);
		}

		/// <inheritdoc/>
		public override async Task<Task<AgentLease?>> AssignLeaseAsync(IAgent agent, CancellationToken cancellationToken)
		{
			Task<(QueueKey, ComputeTaskInfo)?> task = await _taskScheduler.DequeueAsync(queueKey => CheckRequirements(agent, queueKey), cancellationToken);
			return WaitForLeaseAsync(agent, task, cancellationToken);
		}

		private async Task<AgentLease?> WaitForLeaseAsync(IAgent agent, Task<(QueueKey, ComputeTaskInfo)?> task, CancellationToken cancellationToken)
		{
			for (; ; )
			{
				(QueueKey, ComputeTaskInfo)? entry = await task;
				if (entry == null)
				{
					return null;
				}

				AgentLease? lease = await CreateLeaseForEntryAsync(agent, entry.Value);
				if (lease != null)
				{
					return lease;
				}

				task = await _taskScheduler.DequeueAsync(queueKey => CheckRequirements(agent, queueKey), cancellationToken);
			}
		}

		private async Task<AgentLease?> CreateLeaseForEntryAsync(IAgent agent, (QueueKey, ComputeTaskInfo) entry)
		{
			(QueueKey queueKey, ComputeTaskInfo taskInfo) = entry;

			ComputeClusterConfig? cluster = await GetClusterAsync(taskInfo.ClusterId);
			if (cluster == null)
			{
				_logger.LogWarning("Invalid cluster '{ClusterId}'; failing task {TaskRefId}", taskInfo.ClusterId, taskInfo.TaskRefId);

				ComputeTaskStatus status = CreateStatus(taskInfo.TaskRefId, ComputeTaskState.Complete);
				status.AgentId = agent.Id.ToString();
				status.Detail = $"Invalid cluster '{taskInfo.ClusterId}'";
				await PostStatusMessageAsync(taskInfo, status);

				return null;
			}

			Requirements? requirements = await GetCachedRequirementsAsync(queueKey);
			if (requirements == null)
			{
				_logger.LogWarning("Unable to fetch requirements {RequirementsHash}", queueKey);

				ComputeTaskStatus status = CreateStatus(taskInfo.TaskRefId, ComputeTaskState.Complete);
				status.AgentId = agent.Id.ToString();
				status.Detail = $"Unable to retrieve requirements '{queueKey}'";
				await PostStatusMessageAsync(taskInfo, status);

				return null;
			}

			ComputeTaskMessage computeTask = new ComputeTaskMessage();
			computeTask.ClusterId = taskInfo.ClusterId.ToString();
			computeTask.ChannelId = taskInfo.ChannelId.ToString();
			computeTask.NamespaceId = cluster.NamespaceId.ToString();
			computeTask.InputBucketId = cluster.RequestBucketId.ToString();
			computeTask.OutputBucketId = cluster.ResponseBucketId.ToString();
			computeTask.RequirementsHash = queueKey.RequirementsHash;
			computeTask.TaskRefId = taskInfo.TaskRefId;
			computeTask.QueuedAt = Timestamp.FromDateTime(taskInfo.QueuedAt);
			computeTask.DispatchedMs = (int)(DateTime.UtcNow - taskInfo.QueuedAt).TotalMilliseconds;
				
			string leaseName = $"Remote action ({taskInfo.TaskRefId})";
			byte[] payload = Any.Pack(computeTask).ToByteArray();

			AgentLease lease = new AgentLease(LeaseId.GenerateNewId(), leaseName, null, null, null, LeaseState.Pending, requirements.Resources, requirements.Exclusive, payload);
			_logger.LogDebug("Created lease {LeaseId} for channel {ChannelId} task {TaskHash} req {RequirementsHash}", lease.Id, computeTask.ChannelId, computeTask.TaskRefId, computeTask.RequirementsHash);
			return lease;
		}

		/// <inheritdoc/>
		public override Task CancelLeaseAsync(IAgent agent, LeaseId leaseId, ComputeTaskMessage message)
		{
			ClusterId clusterId = new ClusterId(message.ClusterId);
			ComputeTaskInfo taskInfo = new ComputeTaskInfo(clusterId, new RefId(new IoHash(message.TaskRefId.ToByteArray())), new ChannelId(message.ChannelId), message.QueuedAt.ToDateTime());
			return _taskScheduler.EnqueueAsync(new QueueKey(clusterId, new IoHash(message.RequirementsHash.ToByteArray())), taskInfo, true);
		}

		/// <inheritdoc/>
		public async Task<List<ComputeTaskStatus>> GetTaskUpdatesAsync(ClusterId clusterId, ChannelId channelId)
		{
			return await _messageQueue.ReadMessagesAsync(GetMessageQueueId(clusterId, channelId));
		}
		
		/// <inheritdoc/>
		public async Task<int> GetNumQueuedTasksForPoolAsync(ClusterId clusterId, IPool pool, CancellationToken cancellationToken = default)
		{
			return await _taskScheduler.GetNumQueuedTasksAsync(queueKey => CheckRequirements(pool, queueKey), cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<List<ComputeTaskStatus>> WaitForTaskUpdatesAsync(ClusterId clusterId, ChannelId channelId, CancellationToken cancellationToken)
		{
			return await _messageQueue.WaitForMessagesAsync(GetMessageQueueId(clusterId, channelId), cancellationToken);
		}

		/// <inheritdoc/>
		public override async Task OnLeaseStartedAsync(IAgent agent, LeaseId leaseId, ComputeTaskMessage computeTask, ILogger logger)
		{
			await base.OnLeaseStartedAsync(agent, leaseId, computeTask, logger);

			ComputeTaskStatus status = CreateStatus(computeTask.TaskRefId, ComputeTaskState.Executing);
			status.AgentId = agent.Id.ToString();
			status.LeaseId = leaseId.ToString();
			await PostStatusMessageAsync(computeTask, status);
		}

		/// <inheritdoc/>
		public override async Task OnLeaseFinishedAsync(IAgent agent, LeaseId leaseId, ComputeTaskMessage computeTask, LeaseOutcome outcome, ReadOnlyMemory<byte> output, ILogger logger)
		{
			await base.OnLeaseFinishedAsync(agent, leaseId, computeTask, outcome, output, logger);

			DateTime queuedAt = computeTask.QueuedAt.ToDateTime();

			ComputeTaskResultMessage message = ComputeTaskResultMessage.Parser.ParseFrom(output.ToArray());

			ComputeTaskStatus status = CreateStatus(computeTask.TaskRefId, ComputeTaskState.Complete);
			status.AgentId = agent.Id.ToString();
			status.LeaseId = leaseId.ToString();
			status.QueueStats = new ComputeTaskQueueStats(queuedAt, computeTask.DispatchedMs, (int)(DateTime.UtcNow - queuedAt).TotalMilliseconds);
			status.ExecutionStats = message.ExecutionStats?.ToNative();

			if (message.ResultRefId != null)
			{
				status.ResultRefId = message.ResultRefId;
			}
			else if ((ComputeTaskOutcome)message.Outcome != ComputeTaskOutcome.Success)
			{
				(status.Outcome, status.Detail) = ((ComputeTaskOutcome)message.Outcome, message.Detail);
			}
			else if (outcome == LeaseOutcome.Failed)
			{
				status.Outcome = ComputeTaskOutcome.Failed;
			}
			else if (outcome == LeaseOutcome.Cancelled)
			{
				status.Outcome = ComputeTaskOutcome.Cancelled;
			}
			else
			{
				status.Outcome = ComputeTaskOutcome.NoResult;
			}

			logger.LogInformation("Compute lease finished (lease: {LeaseId}, task: {TaskHash}, agent: {AgentId}, channel: {ChannelId}, result: {ResultHash}, outcome: {Outcome})", leaseId, computeTask.TaskRefId.AsRefId(), agent.Id, computeTask.ChannelId, status.ResultRefId?.ToString() ?? "(none)", status.Outcome);
			await PostStatusMessageAsync(computeTask, status);
		}

		/// <summary>
		/// Checks that an agent matches the necessary criteria to execute a task
		/// </summary>
		/// <param name="agent"></param>
		/// <param name="queueKey"></param>
		/// <returns></returns>
		async ValueTask<bool> CheckRequirements(IAgent agent, QueueKey queueKey)
		{
			Requirements? requirements = await GetCachedRequirementsAsync(queueKey);
			if (requirements == null)
			{
				return false;
			}
			return agent.MeetsRequirements(requirements);
		}
		
		/// <summary>
		/// Checks that a pool matches the necessary criteria to execute a task
		/// </summary>
		/// <param name="pool"></param>
		/// <param name="queueKey"></param>
		/// <returns></returns>
		async ValueTask<bool> CheckRequirements(IPool pool, QueueKey queueKey)
		{
			Requirements? requirements = await GetCachedRequirementsAsync(queueKey);
			if (requirements == null)
			{
				return false;
			}
			return pool.MeetsRequirements(requirements);
		}

		/// <summary>
		/// Gets the requirements object from the CAS
		/// </summary>
		/// <param name="queueKey">Queue identifier</param>
		/// <returns>Requirements object for the queue</returns>
		async ValueTask<Requirements?> GetCachedRequirementsAsync(QueueKey queueKey)
		{
			Requirements? requirements;
			if (!_requirementsCache.TryGetValue(queueKey.RequirementsHash, out requirements))
			{
				requirements = await GetRequirementsAsync(queueKey);
				if (requirements != null)
				{
					using (ICacheEntry entry = _requirementsCache.CreateEntry(queueKey.RequirementsHash))
					{
						entry.SetSlidingExpiration(TimeSpan.FromMinutes(10.0));
						entry.SetValue(requirements);
					}
				}
			}
			return requirements;
		}

		/// <summary>
		/// Gets the requirements object for a given queue. Fails tasks in the queue if the requirements object is missing.
		/// </summary>
		/// <param name="queueKey">Queue identifier</param>
		/// <returns>Requirements object for the queue</returns>
		async ValueTask<Requirements?> GetRequirementsAsync(QueueKey queueKey)
		{
			Requirements? requirements = null;

			ComputeClusterConfig? clusterConfig = await GetClusterAsync(queueKey.ClusterId);
			if (clusterConfig != null)
			{
				NamespaceId namespaceId = new NamespaceId(clusterConfig.NamespaceId);
				try
				{
					requirements = await _storageClient.ReadBlobAsync<Requirements>(namespaceId, queueKey.RequirementsHash);
				}
				catch (BlobNotFoundException)
				{
				}
				catch (Exception ex)
				{
					_logger.LogError(ex, "Unable to read blob {NamespaceId}/{RequirementsHash} from storage service", clusterConfig.NamespaceId, queueKey.RequirementsHash);
				}
			}

			if (requirements == null)
			{
				_logger.LogWarning("Unable to fetch requirements object for queue {QueueKey}; failing queued tasks.", queueKey);
				for (; ; )
				{
					ComputeTaskInfo? computeTask = await _taskScheduler.DequeueAsync(queueKey);
					if (computeTask == null)
					{
						break;
					}

					ComputeTaskStatus status = CreateStatus(computeTask.TaskRefId, ComputeTaskState.Complete);
					status.Outcome = ComputeTaskOutcome.BlobNotFound;
					status.Detail = $"Missing requirements object {queueKey.RequirementsHash}";
					_logger.LogInformation("Compute task failed due to missing requirements (queue: {QueueKey}, task: {TaskHash}, channel: {ChannelId})", queueKey, computeTask.TaskRefId, computeTask.ChannelId);
					await PostStatusMessageAsync(computeTask, status);
				}
			}

			return requirements;
		}

		/// <summary>
		/// Post a status message for a particular task
		/// </summary>
		/// <param name="computeTask">The compute task instance</param>
		/// <param name="status">New status for the task</param>
		async Task PostStatusMessageAsync(ComputeTaskInfo computeTask, ComputeTaskStatus status)
		{
			await _messageQueue.PostAsync(GetMessageQueueId(computeTask.ClusterId, computeTask.ChannelId), status);
		}

		/// <summary>
		/// Post a status message for a particular task
		/// </summary>
		/// <param name="computeTaskMessage">The compute task lease</param>
		/// <param name="status">New status for the task</param>
		/// <returns></returns>
		async Task PostStatusMessageAsync(ComputeTaskMessage computeTaskMessage, ComputeTaskStatus status)
		{
			await _messageQueue.PostAsync(GetMessageQueueId(new ClusterId(computeTaskMessage.ClusterId), new ChannelId(computeTaskMessage.ChannelId)), status);
		}

		/// <summary>
		/// Gets the name of a particular message queue
		/// </summary>
		/// <param name="clusterId">The compute cluster</param>
		/// <param name="channelId">Identifier for the message channel</param>
		/// <returns>Name of the message queue</returns>
		static string GetMessageQueueId(ClusterId clusterId, ChannelId channelId)
		{
			return $"{clusterId}/{channelId}";
		}
	}
}
