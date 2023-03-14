// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using EpicGames.Core;
using Horde.Build.Agents.Pools;
using Horde.Build.Agents.Fleet;
using Horde.Build.Projects;
using Horde.Build.Streams;
using Horde.Build.Jobs;
using Horde.Build.Jobs.Graphs;
using Horde.Build.Server;
using Horde.Build.Utilities;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Horde.Build.Agents;

namespace Horde.Build.Tests.Fleet
{
	using PoolId = StringId<IPool>;
	using ProjectId = StringId<IProject>;
	using StreamId = StringId<IStream>;

	[TestClass]
	public class JobQueueStrategyTest : TestSetup
	{
		[TestMethod]
		public async Task GetPoolQueueSizes()
		{
			(JobQueueStrategy strategy, PoolSizeData poolSizeData) = await SetUpJobsAsync(1, 5);
			await Clock.AdvanceAsync(strategy.ReadyTimeThreshold + TimeSpan.FromSeconds(5));
			Dictionary<PoolId, int> poolQueueSizes = await strategy.GetPoolQueueSizesAsync(Clock.UtcNow - TimeSpan.FromHours(2));
			Assert.AreEqual(1, poolQueueSizes.Count);
			Assert.AreEqual(5, poolQueueSizes[poolSizeData.Pool.Id]);
		}
		
		[TestMethod]
		public async Task EmptyJobQueue()
		{
			await AssertAgentCount(0, -1, false);
		}
		
		[TestMethod]
		public async Task EmptyJobQueueWithLargePool()
		{
			await AssertAgentCount(0, -2, false, 20);
		}
		
		[TestMethod]
		public async Task NoAgentsInPool()
		{
			await AssertAgentCount(1, 1, true, 0);
		}
		
		[TestMethod]
		public async Task BatchesNotWaitingLongEnough()
		{
			await AssertAgentCount(3, -1, false);
		}
		
		[TestMethod]
		public async Task NumQueuedJobs1()
		{
			await AssertAgentCount(1, 1, true);
		}
		
		[TestMethod]
		public async Task NumQueuedJobs3()
		{
			await AssertAgentCount(3, 1, true);
		}
		
		[TestMethod]
		public async Task NumQueuedJobs6()
		{
			await AssertAgentCount(6, 2);
		}
		
		[TestMethod]
		public async Task NumQueuedJobs25()
		{
			await AssertAgentCount(25, 7);
		}

		public async Task AssertAgentCount(int numBatchesReady, int expectedAgentDelta, bool waitedBeyondThreshold = true, int numAgents = 8)
		{
			(JobQueueStrategy strategy, PoolSizeData poolSizeData) = await SetUpJobsAsync(1, numBatchesReady, numAgents);
			TimeSpan timeToWait = waitedBeyondThreshold
				? strategy.ReadyTimeThreshold + TimeSpan.FromSeconds(5)
				: TimeSpan.FromSeconds(15);
			
			await Clock.AdvanceAsync(timeToWait);

			List<PoolSizeData> result = await strategy.CalcDesiredPoolSizesAsync(new() { poolSizeData });
			Assert.AreEqual(1, result.Count);
			Assert.AreEqual(result[0].Agents.Count + expectedAgentDelta, result[0].DesiredAgentCount);
		}
	
		/// <summary>
		/// Set up a fixture for job queue tests, ensuring a certain number of job batches are in running or waiting state
		/// </summary>
		/// <param name="numBatchesRunning">Num of job batches that should be in state running</param>
		/// <param name="numBatchesReady">Num of job batches that should be in state waiting</param>
		private async Task<(JobQueueStrategy, PoolSizeData)> SetUpJobsAsync(int numBatchesRunning, int numBatchesReady, int numAgents = 8)
		{
			IPool pool1 = await PoolService.CreatePoolAsync("bogusPool1", null, true, 0, 0);
			List<IAgent> agents = new();
			for (int i = 0; i < numAgents; i++)
			{
				agents.Add(await CreateAgentAsync(pool1));
			}
			
			PoolSizeData poolSize = new (pool1, agents, null);
			
			string agentTypeName1 = "bogusAgentType1";
			Dictionary<string, CreateAgentTypeRequest> agentTypes = new() { {agentTypeName1, new() { Pool = pool1.Name} }, };

			IStream stream = (await CreateOrReplaceStreamAsync(
				new StreamId("ue5-main"),
				null,
				new ProjectId("does-not-exist"),
				new StreamConfig { Name = "//UE5/Main", AgentTypes = agentTypes }
			))!;

			string nodeForAgentType1 = "bogusNodeOnAgentType1";
			IGraph graph = await GraphCollection.AppendAsync(null, new()
			{
				new NewGroup(agentTypeName1, new List<NewNode>
				{
					new (nodeForAgentType1),
				})
			});

			for (int i = 0; i < numBatchesRunning; i++)
			{
				IJob job = await AddPlaceholderJob(graph, stream.Id, nodeForAgentType1);
				await JobCollection.TryUpdateBatchAsync(job, graph, job.Batches[0].Id, null, JobStepBatchState.Running, null);
			}
			
			for (int i = 0; i < numBatchesReady; i++)
			{
				IJob job = await AddPlaceholderJob(graph, stream.Id, nodeForAgentType1);
				await JobCollection.TryUpdateBatchAsync(job, graph, job.Batches[0].Id, null, JobStepBatchState.Ready, null);
			}
			
			return (new (JobCollection, GraphCollection, StreamService, Clock), poolSize);
		}
		
		private async Task<IJob> AddPlaceholderJob(IGraph graph, StreamId streamId, string nodeNameToExecute)
		{
			IJob job = await JobCollection.AddAsync(ObjectId<IJob>.GenerateNewId(), streamId,
				new StringId<TemplateRef>("bogusTemplateRefId"), ContentHash.Empty, graph, "bogusJobName",
				1000, 1000, null, null, null, null, null, null, null, null, null, false,
				false, null, null, new List<string> { "-Target=" + nodeNameToExecute });

			return job;
		}
	}
}