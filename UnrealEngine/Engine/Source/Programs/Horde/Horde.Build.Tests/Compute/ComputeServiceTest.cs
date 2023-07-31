// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Compute;
using EpicGames.Horde.Storage;
using EpicGames.Serialization;
using Horde.Build.Agents.Pools;
using Horde.Build.Server;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Build.Tests.Compute
{
	[TestClass]
	public class ComputeServiceTest : TestSetup
	{
		private readonly ComputeClusterConfig _clusterConfig;
		private readonly NamespaceId _namespaceId;
		private readonly ClusterId _clusterId;
		private static int s_poolId;
		private static int s_channelId;
		private static int s_refId;

		private IStorageClient StorageClient => ServiceProvider.GetRequiredService<IStorageClient>();

		public ComputeServiceTest()
		{
			_clusterConfig = new();
			_namespaceId = new (_clusterConfig.NamespaceId);
			_clusterId = new (_clusterConfig.Id);
		}

		[TestInitialize]
		public async Task Setup()
		{
			Globals globals = await MongoService.GetGlobalsAsync();
			globals.ComputeClusters.Add(_clusterConfig);
			Assert.IsTrue(await MongoService.TryUpdateSingletonAsync(globals));
		}
		
		[TestMethod]
		public async Task QueueCountForPoolNoMatchingCondition()
		{
			await CreateTaskWithRequirement("memory >= 10");
			IPool pool = await CreatePool(new () { {"cpu", "5"} });
			Assert.AreEqual(0, await ComputeService.GetNumQueuedTasksForPoolAsync(_clusterId, pool));
		}

		[TestMethod]
		public async Task QueueCountForPoolSingleCondition()
		{
			await CreateTaskWithRequirement("memory >= 10");
			IPool pool = await CreatePool(new () { {"memory", "20"} });
			Assert.AreEqual(1, await ComputeService.GetNumQueuedTasksForPoolAsync(_clusterId, pool));
		}
		
		[TestMethod]
		public async Task QueueCountForPoolTwoConditions()
		{
			await CreateTaskWithRequirement("memory >= 10 && cpu < 5");
			IPool pool = await CreatePool(new () { {"memory", "15"}, {"cpu", "4"} });
			Assert.AreEqual(1, await ComputeService.GetNumQueuedTasksForPoolAsync(_clusterId, pool));
		}
		
		[TestMethod]
		public async Task QueueCountForPoolMultipleTasks()
		{
			await CreateTaskWithRequirement("memory >= 10 && cpu < 5");
			await CreateTaskWithRequirement("memory >= 15 && cpu < 5");
			await CreateTaskWithRequirement("memory >= 20 && cpu < 5");

			IPool pool1 = await CreatePool(new () { {"memory", "20"}, {"cpu", "4"} });
			IPool pool2 = await CreatePool(new () { {"memory", "15"}, {"cpu", "4"} });

			Assert.AreEqual(3, await ComputeService.GetNumQueuedTasksForPoolAsync(_clusterId, pool1));
			Assert.AreEqual(2, await ComputeService.GetNumQueuedTasksForPoolAsync(_clusterId, pool2));
		}

		private async Task CreateTaskWithRequirement(string condition, ChannelId? channelId = null, RefId? refId = null)
		{
			if (channelId == null)
			{
				channelId = new ChannelId($"bogusChannelId-{s_channelId}");
				Interlocked.Increment(ref s_channelId);
			}

			if (refId == null)
			{
				refId = new RefId($"bogusRefId-{s_refId}");
				Interlocked.Increment(ref s_refId);
			}

			Requirements requirements = new (condition);
			IoHash reqHash = await StorageClient.WriteBlobAsync(_namespaceId, requirements, CancellationToken.None);
			await ComputeService.AddTasksAsync(_clusterId, channelId.Value, new List<RefId> { refId.Value }, new CbObjectAttachment(reqHash));
		}

		private async Task<IPool> CreatePool(Dictionary<string, string> properties)
		{
			string poolName = $"bogusPool-{s_poolId}";
			Interlocked.Increment(ref s_poolId);
			return await PoolService.CreatePoolAsync(poolName, null, true, 0, 0, properties: properties);
		}
	}
}