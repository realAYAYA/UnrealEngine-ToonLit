// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using EpicGames.Core;
using Horde.Build.Jobs;
using Horde.Build.Jobs.Graphs;
using Horde.Build.Agents;
using Horde.Build.Logs;
using Horde.Build.Agents.Leases;
using Horde.Build.Agents.Pools;
using Horde.Build.Streams;
using Horde.Build.Jobs.Templates;
using Horde.Build.Utilities;
using HordeCommon;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using MongoDB.Bson;
using Moq;
using Horde.Build.Agents.Sessions;

namespace Horde.Build.Tests
{
	using JobId = ObjectId<IJob>;
	using LeaseId = ObjectId<ILease>;
	using LogId = ObjectId<ILogFile>;
	using PoolId = StringId<IPool>;
	using StreamId = StringId<IStream>;
	using TemplateRefId = StringId<TemplateRef>;

	[TestClass]
	public class JobCollectionTests : TestSetup
	{
		static NewGroup AddGroup(List<NewGroup> groups)
		{
			NewGroup @group = new NewGroup("win64", new List<NewNode>());
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

		async Task<IJob> StartBatch(IJob job, IGraph graph, int batchIdx)
		{
			Assert.AreEqual(JobStepBatchState.Ready, job.Batches[batchIdx].State);
			job = Deref(await JobCollection.TryUpdateBatchAsync(job, graph, job.Batches[batchIdx].Id, null, JobStepBatchState.Running, null));
			Assert.AreEqual(JobStepBatchState.Running, job.Batches[batchIdx].State);
			return job;
		}

		async Task<IJob> RunStep(IJob job, IGraph graph, int batchIdx, int stepIdx, JobStepOutcome outcome)
		{
			Assert.AreEqual(JobStepState.Ready, job.Batches[batchIdx].Steps[stepIdx].State);
			job = Deref(await JobCollection.TryUpdateStepAsync(job, graph, job.Batches[batchIdx].Id, job.Batches[batchIdx].Steps[stepIdx].Id, JobStepState.Running, JobStepOutcome.Success));
			Assert.AreEqual(JobStepState.Running, job.Batches[batchIdx].Steps[stepIdx].State);
			job = Deref(await JobCollection.TryUpdateStepAsync(job, graph, job.Batches[batchIdx].Id, job.Batches[batchIdx].Steps[stepIdx].Id, JobStepState.Completed, outcome));
			Assert.AreEqual(JobStepState.Completed, job.Batches[batchIdx].Steps[stepIdx].State);
			Assert.AreEqual(outcome, job.Batches[batchIdx].Steps[stepIdx].Outcome);
			return job;
		}

		[TestMethod]
		public async Task TestStates()
		{
			Mock<ITemplate> templateMock = new Mock<ITemplate>(MockBehavior.Strict);
			templateMock.SetupGet(x => x.InitialAgentType).Returns((string?)null);

			IGraph baseGraph = await GraphCollection.AddAsync(templateMock.Object);

			List<string> arguments = new List<string>();
			arguments.Add("-Target=Publish Client");
			arguments.Add("-Target=Post-Publish Client");

			IJob job = await JobCollection.AddAsync(JobId.GenerateNewId(), new StreamId("ue4-main"), new TemplateRefId("test-build"), ContentHash.SHA1("hello"), baseGraph, "Test job", 123, 123, null, null, null, null, null, null, null, null, null, false, false, null, null, arguments);

			job = await StartBatch(job, baseGraph, 0);
			job = await RunStep(job, baseGraph, 0, 0, JobStepOutcome.Success); // Setup Build

			List<NewGroup> newGroups = new List<NewGroup>();

			NewGroup initialGroup = AddGroup(newGroups);
			AddNode(initialGroup, "Update Version Files", null);
			AddNode(initialGroup, "Compile Editor", new[] { "Update Version Files" });

			NewGroup compileGroup = AddGroup(newGroups);
			AddNode(compileGroup, "Compile Client", new[] { "Update Version Files" });

			NewGroup publishGroup = AddGroup(newGroups);
			AddNode(publishGroup, "Cook Client", new[] { "Compile Editor" }, x => x.RunEarly = true);
			AddNode(publishGroup, "Publish Client", new[] { "Compile Client", "Cook Client" });
			AddNode(publishGroup, "Post-Publish Client", null, x => x.OrderDependencies = new List<string> { "Publish Client" });

			IGraph graph = await GraphCollection.AppendAsync(baseGraph, newGroups, null, null);
			job = Deref(await JobCollection.TryUpdateGraphAsync(job, graph));

			job = await StartBatch(job, graph, 1);
			job = await RunStep(job, graph, 1, 0, JobStepOutcome.Success); // Update Version Files
			job = await RunStep(job, graph, 1, 1, JobStepOutcome.Success); // Compile Editor

			job = await StartBatch(job, graph, 2);
			job = await RunStep(job, graph, 2, 0, JobStepOutcome.Success); // Compile Client

			job = await StartBatch(job, graph, 3);
			job = await RunStep(job, graph, 3, 0, JobStepOutcome.Failure); // Cook Client
			Assert.AreEqual(JobStepState.Skipped, job.Batches[3].Steps[1].State); // Publish Client
			Assert.AreEqual(JobStepState.Skipped, job.Batches[3].Steps[2].State); // Post-Publish Client
		}

		[TestMethod]
		public async Task TryAssignLeaseTest()
		{
			Fixture fixture = await CreateFixtureAsync();

			ObjectId<ISession> sessionId1 = new (ObjectId.GenerateNewId());
			await JobCollection.TryAssignLeaseAsync(fixture.Job1, 0, new PoolId("foo"), fixture.Agent1.Id,
				sessionId1, LeaseId.GenerateNewId(), LogId.GenerateNewId());
			
			ObjectId<ISession> sessionId2 = new (ObjectId.GenerateNewId());
			IJob job = (await JobCollection.GetAsync(fixture.Job1.Id))!;
			await JobCollection.TryAssignLeaseAsync(job, 0, new PoolId("foo"), fixture.Agent1.Id,
				sessionId2, LeaseId.GenerateNewId(), LogId.GenerateNewId());
			
			// Manually verify the log output
		}

		[TestMethod]
		public Task LostLeaseTestWithDependency()
		{
			return LostLeaseTestInternal(true);
		}

		[TestMethod]
		public Task LostLeaseTestWithoutDependency()
		{
			return LostLeaseTestInternal(false);
		}

		public async Task LostLeaseTestInternal(bool hasDependency)
		{
			Mock<ITemplate> templateMock = new Mock<ITemplate>(MockBehavior.Strict);
			templateMock.SetupGet(x => x.InitialAgentType).Returns((string?)null);

			IGraph baseGraph = await GraphCollection.AddAsync(templateMock.Object);

			List<string> arguments = new List<string>();
			arguments.Add("-Target=Step 1");
			arguments.Add("-Target=Step 3");

			IJob job = await JobCollection.AddAsync(JobId.GenerateNewId(), new StreamId("ue4-main"), new TemplateRefId("test-build"), ContentHash.SHA1("hello"), baseGraph, "Test job", 123, 123, null, null, null, null, null, null, null, null, null, false, false, null, null, arguments);

			job = await StartBatch(job, baseGraph, 0);
			job = await RunStep(job, baseGraph, 0, 0, JobStepOutcome.Success); // Setup Build

			List<NewGroup> newGroups = new List<NewGroup>();

			NewGroup initialGroup = AddGroup(newGroups);
			AddNode(initialGroup, "Step 1", null);
			AddNode(initialGroup, "Step 2", hasDependency? new[] { "Step 1" } : null);
			AddNode(initialGroup, "Step 3", new[] { "Step 2" });

			IGraph graph = await GraphCollection.AppendAsync(baseGraph, newGroups, null, null);
			job = Deref(await JobCollection.TryUpdateGraphAsync(job, graph));

			job = await StartBatch(job, graph, 1);
			job = await RunStep(job, graph, 1, 0, JobStepOutcome.Success); // Step 1
			job = await RunStep(job, graph, 1, 1, JobStepOutcome.Success); // Step 2

			// Force an error executing the batch
			job = Deref(await JobCollection.TryUpdateBatchAsync(job, graph, job.Batches[1].Id, null, JobStepBatchState.Complete, JobStepBatchError.Incomplete));

			// Check that it restarted all three nodes
			IJob newJob = (await JobCollection.GetAsync(job.Id))!;
			Assert.AreEqual(3, newJob.Batches.Count);
			Assert.AreEqual(1, newJob.Batches[2].GroupIdx);

			if (hasDependency)
			{
				Assert.AreEqual(3, newJob.Batches[2].Steps.Count);

				Assert.AreEqual(0, newJob.Batches[2].Steps[0].NodeIdx);
				Assert.AreEqual(1, newJob.Batches[2].Steps[1].NodeIdx);
				Assert.AreEqual(2, newJob.Batches[2].Steps[2].NodeIdx);
			}
			else
			{
				Assert.AreEqual(2, newJob.Batches[2].Steps.Count);

				Assert.AreEqual(1, newJob.Batches[2].Steps[0].NodeIdx);
				Assert.AreEqual(2, newJob.Batches[2].Steps[1].NodeIdx);
			}
		}

		[TestMethod]
		public async Task IncompleteBatchAsync()
		{
			Mock<ITemplate> templateMock = new Mock<ITemplate>(MockBehavior.Strict);
			templateMock.SetupGet(x => x.InitialAgentType).Returns((string?)null);

			IGraph baseGraph = await GraphCollection.AddAsync(templateMock.Object);

			List<string> arguments = new List<string>();
			arguments.Add("-Target=Step 1");
			arguments.Add("-Target=Step 3");

			IJob job = await JobCollection.AddAsync(JobId.GenerateNewId(), new StreamId("ue4-main"), new TemplateRefId("test-build"), ContentHash.SHA1("hello"), baseGraph, "Test job", 123, 123, null, null, null, null, null, null, null, null, null, false, false, null, null, arguments);
			Assert.AreEqual(1, job.Batches.Count);

			job = await StartBatch(job, baseGraph, 0);
			job = Deref(await JobCollection.TryUpdateBatchAsync(job, baseGraph, job.Batches[0].Id, null, JobStepBatchState.Complete, JobStepBatchError.Incomplete));

			job = (await JobCollection.GetAsync(job.Id))!;
			Assert.AreEqual(2, job.Batches.Count);
			Assert.AreEqual(JobStepBatchState.Complete, job.Batches[0].State);
			Assert.AreEqual(0, job.Batches[0].Steps.Count);
			Assert.AreEqual(JobStepBatchState.Ready, job.Batches[1].State);
			Assert.AreEqual(1, job.Batches[1].Steps.Count);

			job = Deref(await JobCollection.TryUpdateBatchAsync(job, baseGraph, job.Batches[1].Id, null, JobStepBatchState.Complete, JobStepBatchError.Incomplete));

			job = (await JobCollection.GetAsync(job.Id))!;
			Assert.AreEqual(3, job.Batches.Count);
			Assert.AreEqual(JobStepBatchState.Complete, job.Batches[0].State);
			Assert.AreEqual(0, job.Batches[0].Steps.Count);
			Assert.AreEqual(JobStepBatchState.Complete, job.Batches[1].State);
			Assert.AreEqual(0, job.Batches[1].Steps.Count);
			Assert.AreEqual(JobStepBatchState.Ready, job.Batches[2].State);
			Assert.AreEqual(1, job.Batches[2].Steps.Count);

			job = Deref(await JobCollection.TryUpdateBatchAsync(job, baseGraph, job.Batches[2].Id, null, JobStepBatchState.Complete, JobStepBatchError.Incomplete));

			job = (await JobCollection.GetAsync(job.Id))!;
			Assert.AreEqual(3, job.Batches.Count);
			Assert.AreEqual(JobStepBatchState.Complete, job.Batches[0].State);
			Assert.AreEqual(0, job.Batches[0].Steps.Count);
			Assert.AreEqual(JobStepBatchState.Complete, job.Batches[1].State);
			Assert.AreEqual(0, job.Batches[1].Steps.Count);
			Assert.AreEqual(JobStepBatchState.Complete, job.Batches[2].State);
			Assert.AreEqual(1, job.Batches[2].Steps.Count);
			Assert.AreEqual(JobStepState.Skipped, job.Batches[2].Steps[0].State);
		}

		[TestMethod]
		public async Task IncompleteBatchRunningAsync()
		{
			// Same as IncompleteBatchAsync, but with steps moved to running state

			Mock<ITemplate> templateMock = new Mock<ITemplate>(MockBehavior.Strict);
			templateMock.SetupGet(x => x.InitialAgentType).Returns((string?)null);

			IGraph baseGraph = await GraphCollection.AddAsync(templateMock.Object);

			List<string> arguments = new List<string>();
			arguments.Add("-Target=Step 1");
			arguments.Add("-Target=Step 3");

			IJob job = await JobCollection.AddAsync(JobId.GenerateNewId(), new StreamId("ue4-main"), new TemplateRefId("test-build"), ContentHash.SHA1("hello"), baseGraph, "Test job", 123, 123, null, null, null, null, null, null, null, null, null, false, false, null, null, arguments);
			Assert.AreEqual(1, job.Batches.Count);

			// First retry

			job = await StartBatch(job, baseGraph, 0);
			job = Deref(await JobCollection.TryUpdateStepAsync(job, baseGraph, job.Batches[0].Id, job.Batches[0].Steps[0].Id, newState: JobStepState.Running));
			job = Deref(await JobCollection.TryUpdateBatchAsync(job, baseGraph, job.Batches[0].Id, null, JobStepBatchState.Complete, JobStepBatchError.Incomplete));

			job = (await JobCollection.GetAsync(job.Id))!;
			Assert.AreEqual(2, job.Batches.Count);

			Assert.AreEqual(JobStepBatchState.Complete, job.Batches[0].State);
			Assert.AreEqual(1, job.Batches[0].Steps.Count);
			Assert.AreEqual(JobStepState.Completed, job.Batches[0].Steps[0].State);
			Assert.AreEqual(JobStepError.Incomplete, job.Batches[0].Steps[0].Error);

			Assert.AreEqual(JobStepBatchState.Ready, job.Batches[1].State);
			Assert.AreEqual(1, job.Batches[1].Steps.Count);

			// Second retry

			job = Deref(await JobCollection.TryUpdateStepAsync(job, baseGraph, job.Batches[1].Id, job.Batches[1].Steps[0].Id, newState: JobStepState.Running));
			job = Deref(await JobCollection.TryUpdateBatchAsync(job, baseGraph, job.Batches[1].Id, null, JobStepBatchState.Complete, JobStepBatchError.Incomplete));

			job = (await JobCollection.GetAsync(job.Id))!;
			Assert.AreEqual(3, job.Batches.Count);

			Assert.AreEqual(JobStepBatchState.Complete, job.Batches[0].State);
			Assert.AreEqual(1, job.Batches[0].Steps.Count);
			Assert.AreEqual(JobStepState.Completed, job.Batches[0].Steps[0].State);
			Assert.AreEqual(JobStepError.Incomplete, job.Batches[0].Steps[0].Error);

			Assert.AreEqual(JobStepBatchState.Complete, job.Batches[1].State);
			Assert.AreEqual(1, job.Batches[1].Steps.Count);
			Assert.AreEqual(JobStepState.Completed, job.Batches[1].Steps[0].State);
			Assert.AreEqual(JobStepError.Incomplete, job.Batches[1].Steps[0].Error);

			Assert.AreEqual(JobStepBatchState.Ready, job.Batches[2].State);
			Assert.AreEqual(1, job.Batches[2].Steps.Count);

			// Check it doesn't retry a third time

			job = Deref(await JobCollection.TryUpdateStepAsync(job, baseGraph, job.Batches[2].Id, job.Batches[2].Steps[0].Id, newState: JobStepState.Running));
			job = Deref(await JobCollection.TryUpdateBatchAsync(job, baseGraph, job.Batches[2].Id, null, JobStepBatchState.Complete, JobStepBatchError.Incomplete));

			job = (await JobCollection.GetAsync(job.Id))!;
			Assert.AreEqual(3, job.Batches.Count);

			Assert.AreEqual(JobStepBatchState.Complete, job.Batches[0].State);
			Assert.AreEqual(1, job.Batches[0].Steps.Count);
			Assert.AreEqual(JobStepState.Completed, job.Batches[0].Steps[0].State);
			Assert.AreEqual(JobStepError.Incomplete, job.Batches[0].Steps[0].Error);

			Assert.AreEqual(JobStepBatchState.Complete, job.Batches[1].State);
			Assert.AreEqual(1, job.Batches[1].Steps.Count);
			Assert.AreEqual(JobStepState.Completed, job.Batches[1].Steps[0].State);
			Assert.AreEqual(JobStepError.Incomplete, job.Batches[1].Steps[0].Error);

			Assert.AreEqual(JobStepBatchState.Complete, job.Batches[2].State);
			Assert.AreEqual(1, job.Batches[2].Steps.Count);
			Assert.AreEqual(JobStepState.Completed, job.Batches[2].Steps[0].State);
			Assert.AreEqual(JobStepError.Incomplete, job.Batches[2].Steps[0].Error);
		}
	}
}
