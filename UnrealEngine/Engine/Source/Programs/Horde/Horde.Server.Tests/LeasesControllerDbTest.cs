// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using Horde.Server.Agents;
using Horde.Server.Agents.Leases;
using Horde.Server.Agents.Sessions;
using Horde.Server.Utilities;
using HordeCommon;
using Microsoft.AspNetCore.Mvc;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Server.Tests
{
	/// <summary>
	/// Database-only integration test for LeasesController
	/// </summary>
	[TestClass]
	public class LeasesControllerDbTest : TestSetup
	{
		[TestMethod]
		public async Task FindLeases()
		{
			DateTimeOffset minTime = Clock.UtcNow - TimeSpan.FromMinutes(5);
			DateTimeOffset maxTime = Clock.UtcNow;

			ILease lease1 = await CreateLease(Clock.UtcNow - TimeSpan.FromMinutes(10), TimeSpan.FromMinutes(6));
			ILease lease2 = await CreateLease(Clock.UtcNow - TimeSpan.FromMinutes(7), TimeSpan.FromMinutes(3.1));
			/*ILease outOfTimeWindow = */await CreateLease(Clock.UtcNow - TimeSpan.FromMinutes(7), TimeSpan.FromMinutes(25));
			
			ActionResult<List<object>> res = await LeasesController.FindLeasesAsync(null, null, null, null, minTime, maxTime);
			Assert.AreEqual(2, res.Value!.Count);
			Assert.AreEqual(lease2.Id.ToString(), (res.Value[0] as GetAgentLeaseResponse)!.Id);
			Assert.AreEqual(lease1.Id.ToString(), (res.Value[1] as GetAgentLeaseResponse)!.Id);
		}

		private async Task<ILease> CreateLease(DateTime startTime, TimeSpan duration)
		{
			LeaseId id = LeaseId.GenerateNewId();
			SessionId sessionId = SessionId.GenerateNewId();
			ILease lease = await LeaseCollection.AddAsync(id, "myLease", new AgentId("agent-1"), sessionId, null, null, null, startTime, Array.Empty<byte>());
			await LeaseCollection.TrySetOutcomeAsync(id, startTime + duration, LeaseOutcome.Success, null);
			return lease;
		}
	}
}