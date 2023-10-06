// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Security.Cryptography;
using System.Threading.Tasks;
using EpicGames.Horde.Compute;
using EpicGames.Horde.Compute.Clients;
using EpicGames.Horde.Compute.Transports;
using Google.Protobuf;
using Google.Protobuf.WellKnownTypes;
using Horde.Server.Agents;
using Horde.Server.Agents.Leases;
using HordeCommon;
using HordeCommon.Rpc.Tasks;

namespace Horde.Server.Compute
{
	/// <summary>
	/// Assigns compute leases to agents
	/// </summary>
	public class ComputeService
	{
		readonly IAgentCollection _agentCollection;
		readonly AgentService _agentService;

		/// <summary>
		/// Constructor
		/// </summary>
		public ComputeService(IAgentCollection agentCollection, AgentService agentService)
		{
			_agentCollection = agentCollection;
			_agentService = agentService;
		}

		/// <summary>
		/// Allocates a compute resource
		/// </summary>
		/// <returns></returns>
		public async Task<ComputeResource?> TryAllocateResource(Requirements requirements)
		{
			List<IAgent> agents = await _agentCollection.FindAsync();
			foreach (IAgent agent in agents)
			{
				Dictionary<string, int> assignedResources = new Dictionary<string, int>();
				if (agent.MeetsRequirements(requirements, assignedResources))
				{
					ComputeTask computeTask = CreateComputeTask(assignedResources);

					byte[] payload = Any.Pack(computeTask).ToByteArray();
					AgentLease lease = new AgentLease(LeaseId.GenerateNewId(), "Compute task", null, null, null, LeaseState.Pending, assignedResources, requirements.Exclusive, payload);

					ComputeResource? resource = TryAssign(agent, computeTask);
					if (resource != null)
					{
						IAgent? newAgent = await _agentCollection.TryAddLeaseAsync(agent, lease);
						if (newAgent != null)
						{
							await _agentCollection.PublishUpdateEventAsync(agent.Id);
							await _agentService.CreateLeaseAsync(newAgent, lease);
							return resource;
						}
					}
				}
			}
			return null;
		}

		static ComputeResource? TryAssign(IAgent agent, ComputeTask computeTask)
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

			return new ComputeResource(ip, port, computeTask, agent.Properties);
		}

		static ComputeTask CreateComputeTask(Dictionary<string, int> assignedResources)
		{
			ComputeTask computeTask = new ComputeTask();
			computeTask.Nonce = UnsafeByteOperations.UnsafeWrap(RandomNumberGenerator.GetBytes(ServerComputeClient.NonceLength));
			computeTask.Key = UnsafeByteOperations.UnsafeWrap(AesTransport.CreateKey());
			computeTask.Resources.Add(assignedResources);
			return computeTask;
		}
	}
}
