// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using Horde.Server.Agents.Fleet;
using Horde.Server.Agents.Pools;
using Microsoft.AspNetCore.Mvc;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Server.Tests.Agents.Pools
{
	[TestClass]
	public class PoolsControllerTest : TestSetup
	{
		public PoolsControllerTest()
		{
			UpdateConfig(x => x.Pools.Clear());
		}

		[TestMethod]
		public async Task GetPoolsTestAsync()
		{
#pragma warning disable CS0618 // Type or member is obsolete
			PoolConfig poolConfig = new PoolConfig
			{
				Name = "Pool1",
			};
#pragma warning restore CS0618

			UpdateConfig(config =>
			{
				config.Pools.Clear();
				config.Pools.Add(poolConfig);
			});

			ActionResult<List<object>> rawResult = await PoolsController.GetPoolsAsync();
			Assert.AreEqual(1, rawResult.Value!.Count);
			GetPoolResponse response = (rawResult.Value![0] as GetPoolResponse)!;
			Assert.AreEqual(poolConfig.Id.ToString(), response.Id);
			Assert.AreEqual(poolConfig.Name, response.Name);
		}

		[TestMethod]
		public async Task CreatePoolsTestAsync()
		{
#pragma warning disable CS0618 // Type or member is obsolete
			PoolConfig poolConfig = new PoolConfig
			{
				Name = "Pool1",
				ScaleOutCooldown = TimeSpan.FromSeconds(111),
				ScaleInCooldown = TimeSpan.FromSeconds(222),
				SizeStrategies = new List<PoolSizeStrategyInfo> { new() { Type = PoolSizeStrategy.JobQueue, Condition = "dayOfWeek == 'monday'", Config = @"{""ScaleOutFactor"": 22.0, ""ScaleInFactor"": 33.0}", ExtraAgentCount = 567 } },
				SizeStrategy = PoolSizeStrategy.JobQueue,
				JobQueueSettings = new JobQueueSettings(0.35, 0.85),
				FleetManagers = new List<FleetManagerInfo> { new() { Type = FleetManagerType.AwsReuse, Condition = "dayOfWeek == 'monday'", Config = "{}" } },
			};
#pragma warning restore CS0618

			UpdateConfig(config =>
			{
				config.Pools.Clear();
				config.Pools.Add(poolConfig);
			});

			IPool? pool = await PoolService.GetPoolAsync(poolConfig.Id);
			Assert.IsNotNull(pool);
			Assert.AreEqual(poolConfig.Name, pool.Name);
			Assert.AreEqual(poolConfig.ScaleOutCooldown, pool.ScaleOutCooldown!.Value);
			Assert.AreEqual(poolConfig.ScaleInCooldown, pool.ScaleInCooldown!.Value);
			Assert.AreEqual(poolConfig.JobQueueSettings.ScaleOutFactor, pool.JobQueueSettings!.ScaleOutFactor, 0.0001);
			Assert.AreEqual(poolConfig.JobQueueSettings.ScaleInFactor, pool.JobQueueSettings!.ScaleInFactor, 0.0001);
			Assert.AreEqual(1, pool.SizeStrategies?.Count ?? 0);
			Assert.AreEqual(poolConfig.SizeStrategies[0].Type, pool.SizeStrategies![0].Type);
			Assert.AreEqual(poolConfig.SizeStrategies[0].Condition!.Text, pool.SizeStrategies[0].Condition!.Text);
			Assert.AreEqual(poolConfig.SizeStrategies[0].Config, pool.SizeStrategies[0].Config);
			Assert.AreEqual(poolConfig.SizeStrategies[0].ExtraAgentCount, pool.SizeStrategies[0].ExtraAgentCount);
			Assert.AreEqual(1, pool.FleetManagers?.Count ?? 0);
			Assert.AreEqual(poolConfig.FleetManagers[0].Type, pool.FleetManagers![0].Type);
			Assert.AreEqual(poolConfig.FleetManagers[0].Condition!.Text, pool.FleetManagers[0].Condition!.Text);
			Assert.AreEqual(poolConfig.FleetManagers[0].Config, pool.FleetManagers[0].Config);
		}
	}
}