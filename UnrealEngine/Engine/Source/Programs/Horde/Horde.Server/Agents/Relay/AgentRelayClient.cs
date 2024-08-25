// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Grpc.Core;
using Horde.Common.Rpc;
using Microsoft.Extensions.Logging;

namespace Horde.Server.Agents.Relay;

/// <summary>
/// Relay service mirroring port mappings sent from server
/// These mappings will port forward specific ports to agents sitting behind a firewall.
/// An agent using this relay mode usually has multiple IPs assigned to allow bridging.
/// </summary>
public class AgentRelayClient
{
	/// <summary>
	/// Cooldown after an exception occurs. Primarily set to speed up tests.
	/// </summary>
	public TimeSpan CooldownOnException { get; set; } = TimeSpan.FromSeconds(5);

	/// <summary>
	/// Last received revision number after a long-polling responses has returned
	/// </summary>
	public int RevisionNumber { get; private set; } = -2;

	private readonly string _clusterId;
	private readonly string _agentId;
	private readonly List<string> _ipAddresses;
	private readonly Nftables _nftables;
	private readonly RelayRpc.RelayRpcClient _relayRpcClient;
	private readonly ILogger _logger;

	/// <summary>
	/// Constructor
	/// </summary>
	/// <param name="clusterId">Cluster ID this relay agent belongs to</param>
	/// <param name="agentId">Unique ID of this relay agent</param>
	/// <param name="ipAddresses">IP addresses this relay agent is listening on</param>
	/// <param name="nftables"></param>
	/// <param name="relayRpcClient"></param>
	/// <param name="logger"></param>
	public AgentRelayClient(string clusterId, string agentId, List<string> ipAddresses, Nftables nftables, RelayRpc.RelayRpcClient relayRpcClient, ILogger logger)
	{
		_clusterId = clusterId;
		_agentId = agentId;
		_ipAddresses = ipAddresses;
		_nftables = nftables;
		_relayRpcClient = relayRpcClient;
		_logger = logger;
	}

	/// <summary>
	/// Get a list of port mappings from the server using a streaming response.
	/// Using streaming, the relay service can act immediately once new mappings are sent.
	/// This avoids excessive and repetitive polling, and in turn load on the server.
	/// </summary>
	/// <param name="cancellationToken"></param>
	/// <returns>List of new port mappings</returns>
	public async Task<List<PortMapping>?> GetPortMappingsLongPollAsync(CancellationToken cancellationToken)
	{
		_logger.LogDebug("Long polling for port mappings... (revision={Revision})", RevisionNumber);

		GetPortMappingsRequest request = new() { AgentId = _agentId, ClusterId = _clusterId, RevisionCount = RevisionNumber };
		request.IpAddresses.AddRange(_ipAddresses);
		using AsyncServerStreamingCall<GetPortMappingsResponse> cursor = _relayRpcClient.GetPortMappings(request, null, null, cancellationToken);
		await foreach (GetPortMappingsResponse response in cursor.ResponseStream.ReadAllAsync(cancellationToken))
		{
			if (RevisionNumber == response.RevisionCount)
			{
				_logger.LogDebug("Revision did not change");
				return null;
			}
			RevisionNumber = response.RevisionCount;
			return response.PortMappings.ToList();
		}

		return null;
	}

	/// <summary>
	/// Continuously listen for port mappings from server and apply them
	/// </summary>
	/// <param name="cancellationToken"></param>
	public async Task ListenForPortMappingsAsync(CancellationToken cancellationToken)
	{
		// Server cancels the long poll at its discretion
		while (!cancellationToken.IsCancellationRequested)
		{
			try
			{
				List<PortMapping>? portMappings = await GetPortMappingsLongPollAsync(cancellationToken);
				if (portMappings != null)
				{
					_logger.LogDebug("Received {NumMappings} port mappings...", portMappings.Count);
					await _nftables.ApplyPortForwardingAsync(portMappings);
				}
			}
			catch (OperationCanceledException) when (cancellationToken.IsCancellationRequested)
			{
				break;
			}
			catch (RpcException re) when (re.StatusCode == StatusCode.Cancelled)
			{
				// Re-connect as normal
			}
			catch (Exception e)
			{
				_logger.LogError(e, "Failed getting and applying port mappings");
				await Task.Delay(CooldownOnException, CancellationToken.None);
			}
		}
	}
}