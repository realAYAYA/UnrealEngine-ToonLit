// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using EpicGames.Horde.Agents;
using EpicGames.Horde.Agents.Leases;
using EpicGames.Horde.Agents.Sessions;
using Horde.Server.Agents.Leases;
using Horde.Server.Agents.Sessions;
using Horde.Server.Utilities;
using Microsoft.AspNetCore.Mvc;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Server.Tests.Agents.Leases
{
	/// <summary>
	/// Database-only integration test for LeasesController
	/// </summary>
	[TestClass]
	public class LeasesControllerDbTest : TestSetup
	{
		[TestMethod]
		public async Task FindLeasesAsync()
		{
			DateTimeOffset minTime = Clock.UtcNow - TimeSpan.FromMinutes(5);
			DateTimeOffset maxTime = Clock.UtcNow;

			ILease lease1 = await CreateLeaseAsync(Clock.UtcNow - TimeSpan.FromMinutes(10), TimeSpan.FromMinutes(6));
			ILease lease2 = await CreateLeaseAsync(Clock.UtcNow - TimeSpan.FromMinutes(7), TimeSpan.FromMinutes(3.1));
			/*ILease outOfTimeWindow = */
			await CreateLeaseAsync(Clock.UtcNow - TimeSpan.FromMinutes(7), TimeSpan.FromMinutes(25));

			ActionResult<List<object>> res = await LeasesController.FindLeasesAsync(null, null, null, null, minTime, maxTime);
			Assert.AreEqual(2, res.Value!.Count);
			Assert.AreEqual(lease2.Id, (res.Value[0] as GetAgentLeaseResponse)!.Id);
			Assert.AreEqual(lease1.Id, (res.Value[1] as GetAgentLeaseResponse)!.Id);
		}

		private async Task<ILease> CreateLeaseAsync(DateTime startTime, TimeSpan duration)
		{
			LeaseId id = new LeaseId(BinaryIdUtils.CreateNew());
			SessionId sessionId = SessionIdUtils.GenerateNewId();
			ILease lease = await LeaseCollection.AddAsync(id, null, "myLease", new AgentId("agent-1"), sessionId, null, null, null, startTime, Array.Empty<byte>());
			await LeaseCollection.TrySetOutcomeAsync(id, startTime + duration, LeaseOutcome.Success, null);
			return lease;
		}
	}
}