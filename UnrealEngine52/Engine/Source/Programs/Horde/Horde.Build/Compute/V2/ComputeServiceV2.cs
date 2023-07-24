// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Compute;
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
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc.TagHelpers;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Horde.Build.Compute.V2
{
	using LeaseId = ObjectId<ILease>;

	/// <summary>
	/// Dispatches remote actions. Does not implement any cross-pod communication to satisfy leases; only agents connected to this server instance will be stored.
	/// </summary>
	public sealed class ComputeServiceV2 : TaskSourceBase<ComputeTaskMessageV2>
	{
		class Request
		{
			public string Ip { get; }
			public int Port { get; }
			public ReadOnlyMemory<byte> Nonce { get; }
			public ReadOnlyMemory<byte> AesKey { get; }
			public ReadOnlyMemory<byte> AesIv { get; }

			public Request(string ip, int port, ReadOnlyMemory<byte> nonce, ReadOnlyMemory<byte> aesKey, ReadOnlyMemory<byte> aesIv)
			{
				Ip = ip;
				Port = port;
				Nonce = nonce;
				AesKey = aesKey;
				AesIv = aesIv;
			}
		}

		class RequestQueue
		{
			public IoHash Hash { get; }
			public Requirements Requirements { get; }
			public Queue<Request> Requests { get; } = new Queue<Request>();

			public RequestQueue(IoHash hash, Requirements requirements)
			{
				Hash = hash;
				Requirements = requirements;
			}
		}

		class ClusterInfo
		{
			public Dictionary<IoHash, RequestQueue> Queues { get; } = new Dictionary<IoHash, RequestQueue>();
		}

		readonly object _lockObject = new object();
		readonly IOptionsMonitor<GlobalConfig> _globalConfig;
		readonly Dictionary<ClusterId, ClusterInfo> _clusters = new Dictionary<ClusterId, ClusterInfo>();
		readonly ILogger _logger;

		/// <inheritdoc/>
		public override string Type => "ComputeV2";

		/// <inheritdoc/>
		public override TaskSourceFlags Flags => TaskSourceFlags.None;

		/// <summary>
		/// Constructor
		/// </summary>
		public ComputeServiceV2(IOptionsMonitor<GlobalConfig> globalConfig, ILogger<ComputeServiceV2> logger)
		{
			_globalConfig = globalConfig;
			_logger = logger;
		}

		/// <summary>
		/// Adds a new compute request
		/// </summary>
		/// <param name="clusterId">The compute cluster id</param>
		/// <param name="requirements">Requirements for the agent to serve the request</param>
		/// <param name="ip">IP address to connect to</param>
		/// <param name="port">Remote port to connect to</param>
		/// <param name="nonce">Cryptographic nonce used to identify the request</param>
		/// <param name="aesKey">Key for AES encryption on the compute channel</param>
		/// <param name="aesIv">Initialization vector for AES encryption</param>
		public void AddRequest(ClusterId clusterId, Requirements requirements, string ip, int port, ReadOnlyMemory<byte> nonce, ReadOnlyMemory<byte> aesKey, ReadOnlyMemory<byte> aesIv)
		{
			CbObject obj = CbSerializer.Serialize(requirements);
			IoHash hash = IoHash.Compute(obj.GetView().Span);
			lock (_lockObject)
			{
				ClusterInfo? cluster;
				if (!_clusters.TryGetValue(clusterId, out cluster))
				{
					cluster = new ClusterInfo();
					_clusters.Add(clusterId, cluster);
				}

				RequestQueue? queue;
				if (!cluster.Queues.TryGetValue(hash, out queue))
				{
					queue = new RequestQueue(hash, requirements);
					cluster.Queues.Add(hash, queue);
				}

				queue.Requests.Enqueue(new Request(ip, port, nonce, aesKey, aesIv));
			}
		}

		/// <inheritdoc/>
		public override async Task<Task<AgentLease?>> AssignLeaseAsync(IAgent agent, CancellationToken cancellationToken)
		{
			while (!cancellationToken.IsCancellationRequested)
			{
				Request? request = null;
				RequestQueue? requestQueue = null;

				lock (_lockObject)
				{
					ClusterId? removeClusterId = null;

					GlobalConfig globalConfig = _globalConfig.CurrentValue;
					foreach ((ClusterId clusterId, ClusterInfo clusterInfo) in _clusters)
					{
						ComputeClusterConfig? clusterConfig;
						if (!globalConfig.TryGetComputeCluster(clusterId, out clusterConfig))
						{
							removeClusterId ??= clusterId;
						}
						else if (clusterConfig.Condition == null || agent.SatisfiesCondition(clusterConfig.Condition))
						{
							foreach ((IoHash hash, RequestQueue queue) in clusterInfo.Queues)
							{
								if (agent.MeetsRequirements(queue.Requirements))
								{
									request = queue.Requests.Dequeue();
									requestQueue = queue;

									if (queue.Requests.Count == 0)
									{
										clusterInfo.Queues.Remove(hash);
									}
									break;
								}
							}

							if (clusterInfo.Queues.Count == 0)
							{
								removeClusterId ??= clusterId;
							}
						}
					}

					if (removeClusterId != null)
					{
						_clusters.Remove(removeClusterId.Value);
					}
				}

				if (request != null)
				{
					ComputeTaskMessageV2 computeTask = new ComputeTaskMessageV2();
					computeTask.RemoteIp = request.Ip;
					computeTask.RemotePort = request.Port;
					computeTask.Nonce = ByteString.CopyFrom(request.Nonce.Span);
					computeTask.AesKey = ByteString.CopyFrom(request.AesKey.Span);
					computeTask.AesIv = ByteString.CopyFrom(request.AesIv.Span);

					string leaseName = $"Compute lease";
					byte[] payload = Any.Pack(computeTask).ToByteArray();

					AgentLease lease = new AgentLease(LeaseId.GenerateNewId(), leaseName, null, null, null, LeaseState.Pending, requestQueue!.Requirements.Resources, requestQueue.Requirements.Exclusive, payload);
					_logger.LogInformation("Created compute lease for agent {AgentId} and remote {RemoteIp}:{RemotePort}", agent.Id, request.Ip, request.Port);
					return Task.FromResult<AgentLease?>(lease);
				}

				await AsyncUtils.DelayNoThrow(TimeSpan.FromSeconds(5.0), cancellationToken);
			}
			return Task.FromResult<AgentLease?>(null);
		}
	}
}
