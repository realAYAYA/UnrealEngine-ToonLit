// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using Google.Protobuf;
using Google.Protobuf.WellKnownTypes;
using Horde.Server.Agents;
using Horde.Server.Agents.Fleet;
using Horde.Server.Agents.Leases;
using Horde.Server.Streams;
using Horde.Server.Agents.Pools;
using Horde.Server.Utilities;
using HordeCommon;
using HordeCommon.Rpc.Tasks;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Server.Tests.Fleet
{
	[TestClass]
	public class LeaseUtilizationStrategyTest : TestSetup
	{
		private IAgent _agent1 = null!;
		private IAgent _agent2 = null!;
		private IAgent _agent3 = null!;
		private IAgent _agent4 = null!;
		private List<IAgent> _poolAgents = null!;

		public void CreateAgents(IPool pool)
		{
			_agent1 = CreateAgentAsync(pool).Result;
			_agent2 = CreateAgentAsync(pool).Result;
			_agent3 = CreateAgentAsync(pool).Result;
			_agent4 = CreateAgentAsync(pool).Result;
			_poolAgents = new() { _agent1, _agent2, _agent3, _agent4 };
		}

		[TestMethod]
		public async Task UtilizationFull()
		{
			IPool pool = await PoolService.CreatePoolAsync("AutoscalePool1", new AddPoolOptions { EnableAutoscaling = true, MinAgents = 0, NumReserveAgents = 0 });
			CreateAgents(pool);
			
			await AddPlaceholderLease(_agent1, pool, Clock.UtcNow - TimeSpan.FromMinutes(120), TimeSpan.FromMinutes(120));
			await AddPlaceholderLease(_agent2, pool, Clock.UtcNow - TimeSpan.FromMinutes(120), TimeSpan.FromMinutes(120));
			await AddPlaceholderLease(_agent3, pool, Clock.UtcNow - TimeSpan.FromMinutes(120), TimeSpan.FromMinutes(120));
			await AddPlaceholderLease(_agent4, pool, Clock.UtcNow - TimeSpan.FromMinutes(120), TimeSpan.FromMinutes(120));
			await AssertPoolSizeAsync(pool, 4);
		}
		
		[TestMethod]
		public async Task UtilizationHalf()
		{
			IPool pool = await PoolService.CreatePoolAsync("AutoscalePool1", new AddPoolOptions { EnableAutoscaling = true, MinAgents = 0, NumReserveAgents = 0 });
			CreateAgents(pool);
			
			await AddPlaceholderLease(_agent1, pool, Clock.UtcNow - TimeSpan.FromMinutes(120), TimeSpan.FromMinutes(120));
			await AddPlaceholderLease(_agent2, pool, Clock.UtcNow - TimeSpan.FromMinutes(120), TimeSpan.FromMinutes(120));
			await AssertPoolSizeAsync(pool, 2);
		}
		
		[TestMethod]
		public async Task UtilizationZero()
		{
			IPool pool = await PoolService.CreatePoolAsync("AutoscalePool1", new AddPoolOptions { EnableAutoscaling = true, MinAgents = 0, NumReserveAgents = 0 });
			CreateAgents(pool);

			await AssertPoolSizeAsync(pool, 0);
		}
		
		[TestMethod]
		public async Task ReserveAgents()
		{
			IPool pool = await PoolService.CreatePoolAsync("AutoscalePool1", new AddPoolOptions { EnableAutoscaling = true, MinAgents = 0, NumReserveAgents = 5 });
			CreateAgents(pool);
			
			await AddPlaceholderLease(_agent1, pool, Clock.UtcNow - TimeSpan.FromMinutes(120), TimeSpan.FromMinutes(120));
			await AddPlaceholderLease(_agent2, pool, Clock.UtcNow - TimeSpan.FromMinutes(120), TimeSpan.FromMinutes(120));
			await AddPlaceholderLease(_agent3, pool, Clock.UtcNow - TimeSpan.FromMinutes(120), TimeSpan.FromMinutes(120));
			await AddPlaceholderLease(_agent4, pool, Clock.UtcNow - TimeSpan.FromMinutes(120), TimeSpan.FromMinutes(120));
			
			// Full utilization should mean all agents plus the reserve agents
			await AssertPoolSizeAsync(pool, 9);
		}
		
		[TestMethod]
		public async Task MinAgents()
		{
			IPool pool = await PoolService.CreatePoolAsync("AutoscalePool1", new AddPoolOptions { EnableAutoscaling = true, MinAgents = 2, NumReserveAgents = 0 });
			CreateAgents(pool);
			
			// Even with no utilization, expect at least the min number of agents
			await AssertPoolSizeAsync(pool, 2);
		}

		private async Task AssertPoolSizeAsync(IPool pool, int expectedNumAgents)
		{
			IPoolSizeStrategy strategy = FleetService.CreatePoolSizeStrategy(pool);
			PoolSizeResult output = await strategy.CalculatePoolSizeAsync(pool, _poolAgents);
			Assert.AreEqual(expectedNumAgents, output.DesiredAgentCount);
		}
		
		private async Task<ILease> AddPlaceholderLease(IAgent agent, IPool pool, DateTime startTime, TimeSpan duration)
		{
			Assert.IsNotNull(agent.SessionId);
			
			ExecuteJobTask placeholderJobTask = new();
			placeholderJobTask.JobName = "placeholderJobName";
			byte[] payload = Any.Pack(placeholderJobTask).ToByteArray();

			ILease lease = await LeaseCollection.AddAsync(LeaseId.GenerateNewId(), "placeholderLease", agent.Id, agent.SessionId!.Value, new StreamId("placeholderStream"), pool.Id, null, startTime, payload);
			bool wasModified = await LeaseCollection.TrySetOutcomeAsync(lease.Id, startTime + duration, LeaseOutcome.Success, null);
			Assert.IsTrue(wasModified);
			return lease;
		}
	}
}