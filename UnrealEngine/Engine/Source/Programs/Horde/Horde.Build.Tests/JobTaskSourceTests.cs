// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using Horde.Build.Jobs;
using Horde.Build.Jobs.Graphs;
using Horde.Build.Users;
using Horde.Build.Utilities;
using HordeCommon;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System.Linq;
using System;
using Horde.Build.Agents;
using Horde.Build.Agents.Pools;
using Horde.Build.Streams;

namespace Horde.Build.Tests
{
	using JobId = ObjectId<IJob>;
	using UserId = ObjectId<IUser>;

	[TestClass]
	public class JobTaskSourceTests : TestSetup
	{
		private bool _eventReceived;
		private bool? _eventPoolHasAgentsOnline;

		static NewGroup AddGroup(List<NewGroup> groups)
		{
			NewGroup @group = new NewGroup("Win64", new List<NewNode>());
			groups.Add(@group);
			return @group;
		}

		static NewNode AddNode(NewGroup @group, string name, string[]? inputDependencies, Action<NewNode>? action = null)
		{
			NewNode node = new NewNode(name, inputDependencies?.ToList(), inputDependencies?.ToList(), null, null, null, null, null, null);
			if (action != null)
			{
				action.Invoke(node);
			}
			@group.Nodes.Add(node);
			return node;
		}

		[TestMethod]
		public async Task UpdateJobQueueNormal()
		{
			Fixture fixture = await SetupPoolWithAgentAsync(isPoolAutoScaled: true, shouldCreateAgent: true, isAgentEnabled: true);

			Assert.AreEqual(0, JobTaskSource.GetQueueForTesting().Count);
			await JobTaskSource.TickAsync(CancellationToken.None);
			Assert.AreEqual(1, JobTaskSource.GetQueueForTesting().Count);
			Assert.AreEqual(fixture.Job1.Id, JobTaskSource.GetQueueForTesting().Min!.Id.Item1);
			Assert.AreEqual(JobStepBatchState.Ready, JobTaskSource.GetQueueForTesting().Min!.Batch.State);
			
			Assert.IsTrue(_eventReceived);
			Assert.IsTrue(_eventPoolHasAgentsOnline!.Value);
		}
		
		[TestMethod]
		public async Task UpdateJobQueueWithNoAgentsInPool()
		{
			Fixture fixture = await SetupPoolWithAgentAsync(isPoolAutoScaled: true, shouldCreateAgent: false, isAgentEnabled: false);
			
			Assert.AreEqual(0, JobTaskSource.GetQueueForTesting().Count);
			await JobTaskSource.TickAsync(CancellationToken.None);
			Assert.AreEqual(0, JobTaskSource.GetQueueForTesting().Count);

			IJob job = (await JobService.GetJobAsync(fixture.Job1.Id))!;
			Assert.AreEqual(JobStepBatchError.NoAgentsInPool, job.Batches[0].Error);
			
			Assert.IsFalse(_eventReceived);
		}
		
		[TestMethod]
		public async Task UpdateJobQueueWithNoAgentsOnlineInPool()
		{
			Fixture fixture = await SetupPoolWithAgentAsync(isPoolAutoScaled: false, shouldCreateAgent: true, isAgentEnabled: false);
			
			Assert.AreEqual(0, JobTaskSource.GetQueueForTesting().Count);
			await JobTaskSource.TickAsync(CancellationToken.None);
			Assert.AreEqual(0, JobTaskSource.GetQueueForTesting().Count);

			IJob job = (await JobService.GetJobAsync(fixture.Job1.Id))!;
			Assert.AreEqual(JobStepBatchError.NoAgentsOnline, job.Batches[0].Error);
			
			Assert.IsFalse(_eventReceived);
		}
		
		[TestMethod]
		public async Task UpdateJobQueueWithNoAgentsOnlineInAutoScaledPool()
		{
			Fixture fixture = await SetupPoolWithAgentAsync(isPoolAutoScaled: true, shouldCreateAgent: true, isAgentEnabled: false);
			
			Assert.AreEqual(0, JobTaskSource.GetQueueForTesting().Count);
			await JobTaskSource.TickAsync(CancellationToken.None);
			Assert.AreEqual(1, JobTaskSource.GetQueueForTesting().Count);

			Assert.AreEqual(fixture.Job1.Id, JobTaskSource.GetQueueForTesting().Min!.Id.Item1);
			Assert.AreEqual(JobStepBatchState.Ready, JobTaskSource.GetQueueForTesting().Min!.Batch.State);

			Assert.IsTrue(_eventReceived);
			Assert.IsFalse(_eventPoolHasAgentsOnline!.Value);
		}

		[TestMethod]
		public async Task UpdateJobQueueWithPausedStep()
		{
			Fixture fixture = await SetupPoolWithAgentAsync(isPoolAutoScaled: true, shouldCreateAgent: true, isAgentEnabled: true);

			// update template with some step states
			IStream Stream = Deref(await StreamService.TryUpdateTemplateRefAsync(fixture.Stream!, fixture.TemplateRefId1, new List<UpdateStepStateRequest>() { new UpdateStepStateRequest() { Name = "Paused Step", PausedByUserId = UserId.GenerateNewId().ToString() } }));

			// create a new graph with the associated nodes
			List<NewGroup> newGroups = new List<NewGroup>();

			NewGroup initialGroup = AddGroup(newGroups);			
			AddNode(initialGroup, "Update Version Files", null);
			AddNode(initialGroup, "Paused Step", new[] { "Update Version Files" });
			AddNode(initialGroup, "Step That Depends on Paused Step", new[] { "Paused Step" });
			AddNode(initialGroup, "Step That Depends on Update Version Files", new[] { "Update Version Files" });

			IGraph graph = await GraphCollection.AppendAsync(null, newGroups);

			// remove the default fixture jobs
			List<IJob> Jobs = await JobCollection.FindAsync();
			for (int i = 0; i < Jobs.Count; i++)
			{
				await JobCollection.RemoveAsync(Jobs[i]);
			}

			// create a new job
			IJob job = await JobCollection.AddAsync(JobId.GenerateNewId(), Stream.Id,
				fixture.TemplateRefId1, fixture.Template.Id, graph, "Test Paused Step Job",
				1000, 1000, null, null, null, null, Priority.Highest, null, null, null, null, false,
				false, null, null, new List<string> { "-Target=" + "Step That Depends on Paused Step;Step That Depends on Update Version Files" });

			// validate
			Assert.AreEqual(0, JobTaskSource.GetQueueForTesting().Count);
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
			IPool pool = await PoolService.CreatePoolAsync(Fixture.PoolName, null, isPoolAutoScaled, 0, 0);

			if (shouldCreateAgent)
			{
				IAgent? agent = await AgentService.CreateAgentAsync("TestAgent", isAgentEnabled, null, new List<StringId<IPool>> { pool.Id });
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
