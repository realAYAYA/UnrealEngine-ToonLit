// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using EpicGames.Horde.Agents;
using EpicGames.Horde.Agents.Leases;
using EpicGames.Horde.Agents.Pools;
using Horde.Server.Agents;
using Horde.Server.Agents.Sessions;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Server.Tests.Agents;

[TestClass]
public class AgentCollectionTests : TestSetup
{
	private IAgent _agent;
	private readonly AgentLease _lease1;
	private readonly AgentLease _lease2;
	private readonly AgentLease _leaseWithParent3;
	private readonly AgentLease _leaseWithParent4;

	public AgentCollectionTests()
	{
		_agent = CreateAgentAsync(new PoolId("pool1"), ephemeral: true).Result;
		_lease1 = new AgentLease(LeaseId.Parse("aaaaaaaaaaaaaaaaaaaaaaaa"), null, "lease", null, null, null, LeaseState.Pending, null, false, null);
		_lease2 = new AgentLease(LeaseId.Parse("bbbbbbbbbbbbbbbbbbbbbbbb"), null, "lease", null, null, null, LeaseState.Pending, null, false, null);
		_leaseWithParent3 = new AgentLease(LeaseId.Parse("cccccccccccccccccccccccc"), _lease1.Id, "leaseWithParent3", null, null, null, LeaseState.Pending, null, false, null);
		_leaseWithParent4 = new AgentLease(LeaseId.Parse("dddddddddddddddddddddddd"), _lease1.Id, "leaseWithParent4", null, null, null, LeaseState.Pending, null, false, null);
	}

	[TestMethod]
	public async Task AddLeaseAsync()
	{
		await AgentCollection.TryAddLeaseAsync(_agent, _lease1);
		await UpdateAgentAsync();

		await AgentCollection.TryAddLeaseAsync(_agent, _leaseWithParent3);
		await UpdateAgentAsync();

		await AgentCollection.TryAddLeaseAsync(_agent, _leaseWithParent4);
		await UpdateAgentAsync();

		List<LeaseId> leases = await AgentCollection.FindActiveLeaseIdsAsync();

		Assert.AreEqual(3, leases.Count);
		Assert.IsTrue(leases.Contains(_lease1.Id));
		Assert.IsTrue(leases.Contains(_leaseWithParent3.Id));
		Assert.IsTrue(leases.Contains(_leaseWithParent4.Id));
	}

	[TestMethod]
	public async Task StartSessionAsync()
	{
		await AgentCollection.TryAddLeaseAsync(_agent, _lease1);
		await UpdateAgentAsync();

		await AgentCollection.TryAddLeaseAsync(_agent, _lease2);
		await UpdateAgentAsync();

		await AgentCollection.TryStartSessionAsync(_agent, SessionIdUtils.GenerateNewId(), DateTime.UtcNow, AgentStatus.Ok,
			new List<string>(), new Dictionary<string, int>(), new List<PoolId>(), new List<PoolId>(), DateTime.UtcNow, null);

		List<LeaseId> leases = await AgentCollection.FindActiveLeaseIdsAsync();

		Assert.AreEqual(0, leases.Count);
	}

	[TestMethod]
	public async Task UpdateSession_WithEmptyLeasesAsync()
	{
		await AgentCollection.TryAddLeaseAsync(_agent, _lease1);
		await UpdateAgentAsync();

		await AgentCollection.TryAddLeaseAsync(_agent, _lease2);
		await UpdateAgentAsync();

		await AgentCollection.TryUpdateSessionAsync(_agent, null, null, null, null, null, new List<AgentLease>());

		List<LeaseId> leases = await AgentCollection.FindActiveLeaseIdsAsync();

		Assert.AreEqual(0, leases.Count);
	}

	[TestMethod]
	public async Task UpdateSession_WithNewLeasesAsync()
	{
		await AgentCollection.TryAddLeaseAsync(_agent, _lease1);
		await UpdateAgentAsync();

		await AgentCollection.TryUpdateSessionAsync(_agent, null, null, null, null, null, new List<AgentLease> { _lease1, _lease2 });

		List<LeaseId> leases = await AgentCollection.FindActiveLeaseIdsAsync();

		Assert.AreEqual(2, leases.Count);
		Assert.IsTrue(leases.Contains(_lease1.Id));
		Assert.IsTrue(leases.Contains(_lease2.Id));
	}

	[TestMethod]
	public async Task UpdateSession_WithOneLeaseRemovedAsync()
	{
		await AgentCollection.TryAddLeaseAsync(_agent, _lease1);
		await UpdateAgentAsync();

		await AgentCollection.TryAddLeaseAsync(_agent, _lease2);
		await UpdateAgentAsync();

		await AgentCollection.TryUpdateSessionAsync(_agent, null, null, null, null, null, new List<AgentLease> { _lease1 });

		List<LeaseId> leases = await AgentCollection.FindActiveLeaseIdsAsync();

		Assert.AreEqual(1, leases.Count);
		Assert.IsTrue(leases.Contains(_lease1.Id));
	}

	[TestMethod]
	public async Task CancelLeaseAsync()
	{
		await AgentCollection.TryAddLeaseAsync(_agent, _lease1);
		await UpdateAgentAsync();

		await AgentCollection.TryAddLeaseAsync(_agent, _lease2);
		await UpdateAgentAsync();

		await AgentCollection.TryCancelLeaseAsync(_agent, 0);

		List<LeaseId> leases = await AgentCollection.FindActiveLeaseIdsAsync();

		Assert.AreEqual(1, leases.Count);
		Assert.IsTrue(leases.Contains(_lease2.Id));
	}

	[TestMethod]
	public async Task TerminateSessionAsync()
	{
		await AgentCollection.TryAddLeaseAsync(_agent, _lease1);
		await UpdateAgentAsync();

		await AgentCollection.TryAddLeaseAsync(_agent, _lease2);
		await UpdateAgentAsync();

		await AgentCollection.TryTerminateSessionAsync(_agent);

		List<LeaseId> leases = await AgentCollection.FindActiveLeaseIdsAsync();

		Assert.AreEqual(0, leases.Count);
	}

	[TestMethod]
	public async Task GetChildLeaseIdsAsync()
	{
		await AgentCollection.TryAddLeaseAsync(_agent, _lease1);
		await UpdateAgentAsync();

		await AgentCollection.TryAddLeaseAsync(_agent, _leaseWithParent3);
		await UpdateAgentAsync();

		await AgentCollection.TryAddLeaseAsync(_agent, _leaseWithParent4);
		await UpdateAgentAsync();

		List<LeaseId> leases = await AgentCollection.GetChildLeaseIdsAsync(_leaseWithParent3.ParentId!.Value);

		Assert.AreEqual(2, leases.Count);
		Assert.IsTrue(leases.Contains(_leaseWithParent3.Id));
		Assert.IsTrue(leases.Contains(_leaseWithParent4.Id));
	}

	private async Task UpdateAgentAsync()
	{
		_agent = (await AgentService.GetAgentAsync(_agent.Id))!;
	}
}