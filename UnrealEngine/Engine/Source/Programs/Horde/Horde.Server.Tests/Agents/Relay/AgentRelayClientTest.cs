// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using Grpc.Core;
using Horde.Common.Rpc;
using Horde.Server.Agents.Relay;
using Horde.Server.Tests.Server;
using Horde.Server.Utilities;
using Microsoft.Extensions.Logging.Abstractions;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Server.Tests.Agents.Relay;

public class TestRelayRpcClient : RelayRpc.RelayRpcClient
{
	private static readonly ServerCallContext s_adminContext = new ServerCallContextStub(HordeClaims.AdminClaim.ToClaim());
	public TestAsyncStreamReader<GetPortMappingsResponse> GetPortMappingsResponses { get; } = new(s_adminContext);

	public override AsyncServerStreamingCall<GetPortMappingsResponse> GetPortMappings(
		GetPortMappingsRequest request, Metadata headers = null!,
		DateTime? deadline = null, CancellationToken cancellationToken = default)
	{
		return new AsyncServerStreamingCall<GetPortMappingsResponse>(
			GetPortMappingsResponses, null!, null!, null!, () => { });
	}
}

[TestClass]
public class AgentRelayClientTest
{
	private readonly TestRelayRpcClient _grpcClient = new();
	private readonly AgentRelayClient _relayClient;

	public AgentRelayClientTest()
	{
		_relayClient = new AgentRelayClient("myCluster", "myAgent", new List<string> { "192.168.1.99" }, Nftables.CreateNull(), _grpcClient, NullLogger<AgentRelayClient>.Instance);
		_relayClient.CooldownOnException = TimeSpan.Zero;
	}

	[TestMethod]
	public async Task GetPortMappingsLongPoll_Results_Async()
	{
		using CancellationTokenSource cts = new(3000);
		_grpcClient.GetPortMappingsResponses.AddMessage(new GetPortMappingsResponse { PortMappings = { NftablesTests.LeaseMap1 }, RevisionCount = 1 });
		_grpcClient.GetPortMappingsResponses.AddMessage(new GetPortMappingsResponse { PortMappings = { NftablesTests.LeaseMap2, NftablesTests.LeaseMap1 }, RevisionCount = 2 });

		List<PortMapping>? pm1 = await _relayClient.GetPortMappingsLongPollAsync(cts.Token);
		Assert.AreEqual(1, pm1!.Count);
		Assert.AreEqual("lease1", pm1[0].LeaseId);
		Assert.AreEqual(1, _relayClient.RevisionNumber);

		List<PortMapping>? pm2 = await _relayClient.GetPortMappingsLongPollAsync(cts.Token);
		Assert.AreEqual(2, pm2!.Count);
		Assert.AreEqual("lease2", pm2[0].LeaseId);
		Assert.AreEqual("lease1", pm2[1].LeaseId);
		Assert.AreEqual(2, _relayClient.RevisionNumber);
	}

	[TestMethod]
	public async Task GetPortMappingsLongPoll_TimesOut_Async()
	{
		using CancellationTokenSource cts = new(3000);
		await Assert.ThrowsExceptionAsync<OperationCanceledException>(() => _relayClient.GetPortMappingsLongPollAsync(cts.Token));
	}

	[TestMethod]
	public async Task ListenForPortMappings_TimesOut_Async()
	{
		using CancellationTokenSource cts = new(3000);

		Task t = _relayClient.ListenForPortMappingsAsync(cts.Token);
		await Task.Delay(100, cts.Token);
		_grpcClient.GetPortMappingsResponses.AddMessage(new GetPortMappingsResponse { PortMappings = { NftablesTests.LeaseMap1 } });
		await Task.Delay(100, cts.Token);
		_grpcClient.GetPortMappingsResponses.AddMessage(new GetPortMappingsResponse { PortMappings = { NftablesTests.LeaseMap2 } });

		await t;
	}
}
