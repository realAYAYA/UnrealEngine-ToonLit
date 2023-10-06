// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Security.Cryptography;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Compute;
using EpicGames.Horde.Compute.Clients;
using EpicGames.Horde.Compute.Transports;
using Google.Protobuf;
using Google.Protobuf.WellKnownTypes;
using Horde.Server.Agents;
using Horde.Server.Agents.Leases;
using Horde.Server.Server;
using Horde.Server.Tasks;
using HordeCommon;
using HordeCommon.Rpc.Tasks;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Horde.Server.Compute
{
	/// <summary>
	/// Information about a compute 
	/// </summary>
	public class ComputeResource
	{
		/// <summary>
		/// IP address of the agent
		/// </summary>
		public IPAddress Ip { get; }

		/// <summary>
		/// Port to connect on
		/// </summary>
		public int Port { get; }

		/// <summary>
		/// Information about the compute task
		/// </summary>
		public ComputeTask Task { get; }

		/// <summary>
		/// Properties of the assigned agent
		/// </summary>
		public IReadOnlyList<string> Properties { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public ComputeResource(IPAddress ip, int port, ComputeTask task, IReadOnlyList<string> properties)
		{
			Ip = ip;
			Port = port;
			Task = task;
			Properties = properties;
		}
	}

	/// <summary>
	/// Dispatches requests for compute resources
	/// </summary>
	public class ComputeTaskSource : TaskSourceBase<ComputeTask>
	{
		class Waiter
		{
			public IAgent Agent { get; }
			public IPAddress Ip { get; }
			public int Port { get; }
			public TaskCompletionSource<AgentLease?> Lease { get; } = new TaskCompletionSource<AgentLease?>(TaskCreationOptions.RunContinuationsAsynchronously);

			public Waiter(IAgent agent, IPAddress ip, int port)
			{
				Agent = agent;
				Ip = ip;
				Port = port;
			}
		}

		/// <inheritdoc/>
		public override string Type => "Compute";

		/// <inheritdoc/>
		public override TaskSourceFlags Flags => TaskSourceFlags.None;

		readonly IOptionsMonitor<GlobalConfig> _globalConfig;
		readonly ILogger _logger;

		readonly object _lockObject = new object();
		readonly Dictionary<ClusterId, LinkedList<Waiter>> _waiters = new Dictionary<ClusterId, LinkedList<Waiter>>();

		/// <summary>
		/// Constructor
		/// </summary>
		public ComputeTaskSource(IOptionsMonitor<GlobalConfig> globalConfig, ILogger<ComputeTaskSource> logger)
		{
			_globalConfig = globalConfig;
			_logger = logger;
		}

		/// <inheritdoc/>
		public override Task<Task<AgentLease?>> AssignLeaseAsync(IAgent agent, CancellationToken cancellationToken)
		{
			return Task.FromResult(WaitInternalAsync(agent, cancellationToken));
		}

		async Task<AgentLease?> WaitInternalAsync(IAgent agent, CancellationToken cancellationToken)
		{
			string? ipStr = agent.GetPropertyValues("ComputeIp").FirstOrDefault();
			if (ipStr == null || !IPAddress.TryParse(ipStr, out IPAddress? ip))
			{
				return null;
			}

			string? portStr = agent.GetPropertyValues("ComputePort").FirstOrDefault();
			if (portStr == null || !Int32.TryParse(portStr, out int port))
			{
				return null;
			}

			// Add it to the wait queue
			List<(LinkedList<Waiter>, LinkedListNode<Waiter>)> nodes = new();
			try
			{
				GlobalConfig globalConfig = _globalConfig.CurrentValue;

				Waiter? waiter = null;
				lock (_lockObject)
				{
					foreach (ComputeClusterConfig clusterConfig in globalConfig.Compute)
					{
						if (clusterConfig.Condition == null || agent.SatisfiesCondition(clusterConfig.Condition))
						{
							LinkedList<Waiter>? list;
							if (!_waiters.TryGetValue(clusterConfig.Id, out list))
							{
								list = new LinkedList<Waiter>();
								_waiters.Add(clusterConfig.Id, list);
							}

							waiter ??= new Waiter(agent, ip, port);
							list.AddFirst(waiter);
						}
					}
				}

				if (waiter != null)
				{
					using (IDisposable disposable = cancellationToken.Register(() => waiter.Lease.TrySetResult(null)))
					{
						AgentLease? lease = await waiter.Lease.Task;
						if (lease != null)
						{
							_logger.LogInformation("Created compute lease for agent {AgentId}", agent.Id);
							return lease;
						}
					}
				}
			}
			finally
			{
				lock (_lockObject)
				{
					foreach ((LinkedList<Waiter> list, LinkedListNode<Waiter> node) in nodes)
					{
						list.Remove(node);
					}
				}
			}

			return null;
		}
	}
}
