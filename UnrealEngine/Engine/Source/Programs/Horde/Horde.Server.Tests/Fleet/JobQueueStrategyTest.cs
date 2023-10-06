// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using EpicGames.Core;
using Horde.Server.Agents.Pools;
using Horde.Server.Agents.Fleet;
using Horde.Server.Projects;
using Horde.Server.Streams;
using Horde.Server.Jobs;
using Horde.Server.Jobs.Graphs;
using Horde.Server.Server;
using Horde.Server.Utilities;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Horde.Server.Agents;
using EpicGames.Horde.Api;

namespace Horde.Server.Tests.Fleet
{
	[TestClass]
	public class JobQueueStrategyTest : TestSetup
	{
		[TestMethod]
		public async Task GetPoolQueueSizes()
		{
			(JobQueueStrategy strategy, PoolSizeResult poolSizeResult, IPool pool, List<IAgent> agents) = await SetUpJobsAsync(1, 5);
			await Clock.AdvanceAsync(TimeSpan.FromSeconds(strategy.Settings.ReadyTimeThresholdSec) + TimeSpan.FromSeconds(5));
			Dictionary<PoolId, int> poolQueueSizes = await strategy.GetPoolQueueSizesAsync(Clock.UtcNow - TimeSpan.FromHours(2));
			Assert.AreEqual(1, poolQueueSizes.Count);
			Assert.AreEqual(5, poolQueueSizes[pool.Id]);
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
			(JobQueueStrategy strategy, PoolSizeResult poolSizeResult, IPool pool, List<IAgent> agents) = await SetUpJobsAsync(1, numBatchesReady, numAgents);
			TimeSpan timeToWait = waitedBeyondThreshold
				? TimeSpan.FromSeconds(strategy.Settings.ReadyTimeThresholdSec) + TimeSpan.FromSeconds(5)
				: TimeSpan.FromSeconds(15);
			
			await Clock.AdvanceAsync(timeToWait);

			PoolSizeResult result = await strategy.CalculatePoolSizeAsync(pool, agents);
			Assert.AreEqual(agents.Count + expectedAgentDelta, result.DesiredAgentCount);
		}
	
		/// <summary>
		/// Set up a fixture for job queue tests, ensuring a certain number of job batches are in running or waiting state
		/// </summary>
		/// <param name="numBatchesRunning">Num of job batches that should be in state running</param>
		/// <param name="numBatchesReady">Num of job batches that should be in state waiting</param>
		private async Task<(JobQueueStrategy, PoolSizeResult, IPool, List<IAgent> agents)> SetUpJobsAsync(int numBatchesRunning, int numBatchesReady, int numAgents = 8)
		{
			IPool pool = await PoolService.CreatePoolAsync("bogusPool1", new AddPoolOptions { EnableAutoscaling = true, MinAgents = 0, NumReserveAgents = 0 });
			List<IAgent> agents = new();
			for (int i = 0; i < numAgents; i++)
			{
				agents.Add(await CreateAgentAsync(pool));
			}
			
			PoolSizeResult poolSize = new (agents.Count, agents.Count, null);
			
			string agentTypeName1 = "bogusAgentType1";
			Dictionary<string, AgentConfig> agentTypes = new() { {agentTypeName1, new() { Pool = new PoolId(pool.Name) } }, };

			StreamConfig streamConfig = new StreamConfig { Id = new StreamId("ue5"), Name = "//UE5/Main", AgentTypes = agentTypes };

			ProjectConfig projectConfig = new ProjectConfig();
			projectConfig.Id = new ProjectId("ue5");
			projectConfig.Streams.Add(streamConfig);

			GlobalConfig globalConfig = new GlobalConfig();
			globalConfig.Projects.Add(projectConfig);

			SetConfig(globalConfig);

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
				IJob job = await AddPlaceholderJob(graph, streamConfig.Id, nodeForAgentType1);
				await JobCollection.TryUpdateBatchAsync(job, graph, job.Batches[0].Id, null, JobStepBatchState.Running, null);
			}
			
			for (int i = 0; i < numBatchesReady; i++)
			{
				IJob job = await AddPlaceholderJob(graph, streamConfig.Id, nodeForAgentType1);
				await JobCollection.TryUpdateBatchAsync(job, graph, job.Batches[0].Id, null, JobStepBatchState.Ready, null);
			}

			return (new (JobCollection, GraphCollection, StreamCollection, Clock, Cache, GlobalConfig), poolSize, pool, agents);
		}
		
		private async Task<IJob> AddPlaceholderJob(IGraph graph, StreamId streamId, string nodeNameToExecute)
		{
			CreateJobOptions options = new CreateJobOptions();
			options.Arguments.Add($"-Target={nodeNameToExecute}");

			IJob job = await JobCollection.AddAsync(JobId.GenerateNewId(), streamId,
				new TemplateId("bogusTemplateRefId"), ContentHash.Empty, graph, "bogusJobName",
				1000, 1000, options);

			return job;
		}
	}
}
