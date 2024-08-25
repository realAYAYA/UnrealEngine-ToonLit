// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Agents;
using EpicGames.Horde.Agents.Pools;
using EpicGames.Horde.Users;
using Horde.Server.Agents;
using Horde.Server.Agents.Pools;
using Horde.Server.Jobs;
using Horde.Server.Jobs.Graphs;
using Horde.Server.Streams;
using Horde.Server.Utilities;
using HordeCommon;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Server.Tests.Jobs
{
	[TestClass]
	public class JobTaskSourceTests : TestSetup
	{
		private bool _eventReceived;
		private bool? _eventPoolHasAgentsOnline;

		static NewGroup AddGroup(List<NewGroup> groups)
		{
			NewGroup group = new NewGroup("Win64", new List<NewNode>());
			groups.Add(group);
			return group;
		}

		static NewNode AddNode(NewGroup group, string name, string[]? inputDependencies, Action<NewNode>? action = null)
		{
			NewNode node = new NewNode(name, inputDependencies: inputDependencies?.ToList(), orderDependencies: inputDependencies?.ToList());
			action?.Invoke(node);
			group.Nodes.Add(node);
			return node;
		}

		[TestMethod]
		public async Task UpdateJobQueueNormalAsync()
		{
			Fixture fixture = await SetupPoolWithAgentAsync(isPoolAutoScaled: true, shouldCreateAgent: true, isAgentEnabled: true);

			await JobTaskSource.TickAsync(CancellationToken.None);
			Assert.AreEqual(1, JobTaskSource.GetQueueForTesting().Count);
			Assert.AreEqual(fixture.Job1.Id, JobTaskSource.GetQueueForTesting().Min!.Id.Item1);
			Assert.AreEqual(JobStepBatchState.Ready, JobTaskSource.GetQueueForTesting().Min!.Batch.State);

			Assert.IsTrue(_eventReceived);
			Assert.IsTrue(_eventPoolHasAgentsOnline!.Value);
		}

		[TestMethod]
		public async Task UpdateJobQueueWithNoAgentsInPoolAsync()
		{
			Fixture fixture = await SetupPoolWithAgentAsync(isPoolAutoScaled: true, shouldCreateAgent: false, isAgentEnabled: false);

			await JobTaskSource.TickAsync(CancellationToken.None);
			Assert.AreEqual(0, JobTaskSource.GetQueueForTesting().Count);

			IJob job = (await JobService.GetJobAsync(fixture.Job1.Id))!;
			Assert.AreEqual(JobStepBatchError.NoAgentsInPool, job.Batches[0].Error);

			Assert.IsFalse(_eventReceived);
		}

		[TestMethod]
		public async Task UpdateJobQueueWithNoAgentsOnlineInPoolAsync()
		{
			Fixture fixture = await SetupPoolWithAgentAsync(isPoolAutoScaled: false, shouldCreateAgent: true, isAgentEnabled: false);

			await JobTaskSource.TickAsync(CancellationToken.None);
			Assert.AreEqual(1, JobTaskSource.GetQueueForTesting().Count);

			Assert.AreEqual(fixture.Job1.Id, JobTaskSource.GetQueueForTesting().Min!.Id.Item1);
			Assert.AreEqual(JobStepBatchState.Ready, JobTaskSource.GetQueueForTesting().Min!.Batch.State);

			Assert.IsTrue(_eventReceived);
			Assert.IsFalse(_eventPoolHasAgentsOnline!.Value);
		}

		[TestMethod]
		public async Task UpdateJobQueueWithNoAgentsOnlineInAutoScaledPoolAsync()
		{
			Fixture fixture = await SetupPoolWithAgentAsync(isPoolAutoScaled: true, shouldCreateAgent: true, isAgentEnabled: false);

			await JobTaskSource.TickAsync(CancellationToken.None);
			Assert.AreEqual(1, JobTaskSource.GetQueueForTesting().Count);

			Assert.AreEqual(fixture.Job1.Id, JobTaskSource.GetQueueForTesting().Min!.Id.Item1);
			Assert.AreEqual(JobStepBatchState.Ready, JobTaskSource.GetQueueForTesting().Min!.Batch.State);

			Assert.IsTrue(_eventReceived);
			Assert.IsFalse(_eventPoolHasAgentsOnline!.Value);
		}

		[TestMethod]
		public async Task UpdateJobQueueWithPausedStepAsync()
		{
			Fixture fixture = await SetupPoolWithAgentAsync(isPoolAutoScaled: true, shouldCreateAgent: true, isAgentEnabled: true);

			// update template with some step states
			IStream stream = await StreamCollection.GetAsync(fixture.StreamConfig!);
			stream = Deref(await StreamCollection.TryUpdateTemplateRefAsync(stream, fixture.TemplateRefId1, new List<UpdateStepStateRequest>() { new UpdateStepStateRequest() { Name = "Paused Step", PausedByUserId = new UserId(BinaryIdUtils.CreateNew()).ToString() } }));

			// create a new graph with the associated nodes
			List<NewGroup> newGroups = new List<NewGroup>();

			NewGroup initialGroup = AddGroup(newGroups);
			AddNode(initialGroup, "Update Version Files", null);
			AddNode(initialGroup, "Paused Step", new[] { "Update Version Files" });
			AddNode(initialGroup, "Step That Depends on Paused Step", new[] { "Paused Step" });
			AddNode(initialGroup, "Step That Depends on Update Version Files", new[] { "Update Version Files" });

			IGraph graph = await GraphCollection.AppendAsync(null, newGroups);

			// remove the default fixture jobs
			IReadOnlyList<IJob> jobs = await JobCollection.FindAsync();
			for (int i = 0; i < jobs.Count; i++)
			{
				await JobCollection.RemoveAsync(jobs[i]);
			}

			// create a new job
			CreateJobOptions options = new CreateJobOptions();
			options.Arguments.Add("-Target=Step That Depends on Paused Step;Step That Depends on Update Version Files");

			IJob job = await JobCollection.AddAsync(JobIdUtils.GenerateNewId(), stream.Id,
				fixture.TemplateRefId1, fixture.Template.Hash, graph, "Test Paused Step Job",
				1000, 1000, options);

			// validate
			await JobTaskSource.TickAsync(CancellationToken.None);
			Assert.AreEqual(1, JobTaskSource.GetQueueForTesting().Count);
			Assert.AreEqual(job.Id, JobTaskSource.GetQueueForTesting().Min!.Id.Item1);
			Assert.AreEqual(JobStepBatchState.Ready, JobTaskSource.GetQueueForTesting().Min!.Batch.State);

			Assert.IsTrue(_eventReceived);
			Assert.IsTrue(_eventPoolHasAgentsOnline!.Value);

			// make sure we get db value
			job = Deref(await JobService.GetJobAsync(job.Id));

			IJobStepBatch batch = job.Batches[0];

			Assert.AreEqual(batch.Steps.Count, 4);
			Assert.AreEqual(batch.Steps[0].State, JobStepState.Ready);
			Assert.AreEqual(batch.Steps[1].State, JobStepState.Skipped);
			Assert.AreEqual(batch.Steps[1].Error, JobStepError.Paused);
			Assert.AreEqual(batch.Steps[2].State, JobStepState.Skipped);
			Assert.AreEqual(batch.Steps[3].State, JobStepState.Waiting);

		}

		private async Task<Fixture> SetupPoolWithAgentAsync(bool isPoolAutoScaled, bool shouldCreateAgent, bool isAgentEnabled)
		{
			Fixture fixture = await CreateFixtureAsync();
			IPool pool = await CreatePoolAsync(new PoolConfig { Name = Fixture.PoolName, EnableAutoscaling = isPoolAutoScaled, MinAgents = 0, NumReserveAgents = 0 });

			if (shouldCreateAgent)
			{
				IAgent? agent = await AgentService.CreateAgentAsync("TestAgent", false, "");
				Assert.IsNotNull(agent);

				agent = await AgentService.Agents.TryUpdateSettingsAsync(agent, enabled: isAgentEnabled, pools: new List<PoolId> { pool.Id });
				Assert.IsNotNull(agent);

				await AgentService.CreateSessionAsync(agent, AgentStatus.Ok, new List<string>(), new Dictionary<string, int>(), null);
			}

			JobTaskSource.OnJobScheduled += (pool, poolHasAgentsOnline, job, graph, batchId) =>
			{
				_eventReceived = true;
				_eventPoolHasAgentsOnline = poolHasAgentsOnline;
			};

			return fixture;
		}
	}
}
