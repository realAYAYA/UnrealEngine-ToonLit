// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using EpicGames.Core;
using Horde.Server.Jobs;
using Horde.Server.Jobs.Graphs;
using Horde.Server.Agents;
using Horde.Server.Logs;
using Horde.Server.Agents.Leases;
using Horde.Server.Agents.Pools;
using Horde.Server.Streams;
using Horde.Server.Jobs.Templates;
using Horde.Server.Utilities;
using HordeCommon;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using MongoDB.Bson;
using Moq;
using Horde.Server.Agents.Sessions;
using Horde.Server.Users;

namespace Horde.Server.Tests
{
	[TestClass]
	public class JobCollectionTests : TestSetup
	{
		static NewGroup AddGroup(List<NewGroup> groups)
		{
			NewGroup group = new NewGroup("win64", new List<NewNode>());
			groups.Add(group);
			return group;
		}

		static NewNode AddNode(NewGroup group, string name, string[]? inputDependencies, Action<NewNode>? action = null)
		{
			NewNode node = new NewNode(name, inputDependencies: inputDependencies?.ToList(), orderDependencies: inputDependencies?.ToList());
			if (action != null)
			{
				action.Invoke(node);
			}
			group.Nodes.Add(node);
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

			IGraph baseGraph = await GraphCollection.AddAsync(templateMock.Object, null);

			CreateJobOptions options = new CreateJobOptions();
			options.Arguments.Add("-Target=Publish Client");
			options.Arguments.Add("-Target=Post-Publish Client");

			IJob job = await JobCollection.AddAsync(JobId.GenerateNewId(), new StreamId("ue4-main"), new TemplateId("test-build"), ContentHash.SHA1("hello"), baseGraph, "Test job", 123, 123, options);

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

			SessionId sessionId1 = SessionId.GenerateNewId();
			await JobCollection.TryAssignLeaseAsync(fixture.Job1, 0, new PoolId("foo"), fixture.Agent1.Id,
				sessionId1, LeaseId.GenerateNewId(), LogId.GenerateNewId());
			
			SessionId sessionId2 = SessionId.GenerateNewId();
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

			IGraph baseGraph = await GraphCollection.AddAsync(templateMock.Object, null);

			CreateJobOptions options = new CreateJobOptions();
			options.Arguments.Add("-Target=Step 1");
			options.Arguments.Add("-Target=Step 3");

			IJob job = await JobCollection.AddAsync(JobId.GenerateNewId(), new StreamId("ue4-main"), new TemplateId("test-build"), ContentHash.SHA1("hello"), baseGraph, "Test job", 123, 123, options);

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

			IGraph baseGraph = await GraphCollection.AddAsync(templateMock.Object, null);

			CreateJobOptions options = new CreateJobOptions();
			options.Arguments.Add("-Target=Step 1");
			options.Arguments.Add("-Target=Step 3");

			IJob job = await JobCollection.AddAsync(JobId.GenerateNewId(), new StreamId("ue4-main"), new TemplateId("test-build"), ContentHash.SHA1("hello"), baseGraph, "Test job", 123, 123, options);
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

			IGraph baseGraph = await GraphCollection.AddAsync(templateMock.Object, null);

			CreateJobOptions options = new CreateJobOptions();
			options.Arguments.Add("-Target=Step 1");
			options.Arguments.Add("-Target=Step 3");

			IJob job = await JobCollection.AddAsync(JobId.GenerateNewId(), new StreamId("ue4-main"), new TemplateId("test-build"), ContentHash.SHA1("hello"), baseGraph, "Test job", 123, 123, options);
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

		[TestMethod]
		public async Task RetryStepSameGroupAsync()
		{
			Mock<ITemplate> templateMock = new Mock<ITemplate>(MockBehavior.Strict);
			templateMock.SetupGet(x => x.InitialAgentType).Returns((string?)null);

			List<NewGroup> newGroups = new List<NewGroup>();

			NewGroup group = AddGroup(newGroups);
			AddNode(group, "Step 1", null);
			AddNode(group, "Step 2", new[] { "Step 1" });

			IGraph graph = await GraphCollection.AppendAsync(null, newGroups, null, null);

			CreateJobOptions options = new CreateJobOptions();
			options.Arguments.Add("-Target=Step 2");

			IJob job = await JobCollection.AddAsync(JobId.GenerateNewId(), new StreamId("ue4-main"), new TemplateId("test-build"), ContentHash.SHA1("hello"), graph, "Test job", 123, 123, options);

			// Fail the first step
			job = await StartBatch(job, graph, 0);
			job = await RunStep(job, graph, 0, 0, JobStepOutcome.Failure);

			Assert.AreEqual(1, job.Batches.Count);
			Assert.AreEqual(2, job.Batches[0].Steps.Count);
			Assert.AreEqual(JobStepState.Completed, job.Batches[0].Steps[0].State);
			Assert.AreEqual(JobStepOutcome.Failure, job.Batches[0].Steps[0].Outcome);
			Assert.AreEqual(JobStepState.Skipped, job.Batches[0].Steps[1].State);
			Assert.AreEqual(JobStepOutcome.Failure, job.Batches[0].Steps[1].Outcome);

			// Retry the failed step (expect: new batch containing the retried step is created, skipped steps from original batch are removed)
			job = Deref(await JobCollection.TryUpdateStepAsync(job, graph, job.Batches[0].Id, job.Batches[0].Steps[0].Id, newRetryByUserId: UserId.Anonymous));
			Assert.AreEqual(2, job.Batches.Count);
			Assert.AreEqual(1, job.Batches[0].Steps.Count);
			Assert.AreEqual(2, job.Batches[1].Steps.Count);

			IJobStep step = job.Batches[0].Steps[0];
			Assert.AreEqual(0, step.NodeIdx);
			Assert.AreEqual(JobStepState.Completed, step.State);
			Assert.AreEqual(JobStepOutcome.Failure, step.Outcome);

			step = job.Batches[1].Steps[0];
			Assert.AreEqual(0, step.NodeIdx);
			Assert.AreEqual(JobStepState.Ready, step.State);
			Assert.AreEqual(JobStepOutcome.Success, step.Outcome);

			step = job.Batches[1].Steps[1];
			Assert.AreEqual(1, step.NodeIdx);
			Assert.AreEqual(JobStepState.Waiting, step.State);
			Assert.AreEqual(JobStepOutcome.Success, step.Outcome);

			// Fail the retried step
			job = await RunStep(job, graph, 1, 0, JobStepOutcome.Failure);
			Assert.AreEqual(2, job.Batches.Count);
			Assert.AreEqual(1, job.Batches[0].Steps.Count);
			Assert.AreEqual(2, job.Batches[1].Steps.Count);

			step = job.Batches[0].Steps[0];
			Assert.AreEqual(0, step.NodeIdx);
			Assert.AreEqual(JobStepState.Completed, step.State);
			Assert.AreEqual(JobStepOutcome.Failure, step.Outcome);

			step = job.Batches[1].Steps[0];
			Assert.AreEqual(0, step.NodeIdx);
			Assert.AreEqual(JobStepState.Completed, step.State);
			Assert.AreEqual(JobStepOutcome.Failure, step.Outcome);

			step = job.Batches[1].Steps[1];
			Assert.AreEqual(1, step.NodeIdx);
			Assert.AreEqual(JobStepState.Skipped, step.State);
			Assert.AreEqual(JobStepOutcome.Failure, step.Outcome);

			// Retry the failed step for a second time
			job = Deref(await JobCollection.TryUpdateStepAsync(job, graph, job.Batches[1].Id, job.Batches[1].Steps[0].Id, newRetryByUserId: UserId.Anonymous));
			Assert.AreEqual(3, job.Batches.Count);
			Assert.AreEqual(1, job.Batches[0].Steps.Count);
			Assert.AreEqual(1, job.Batches[1].Steps.Count);
			Assert.AreEqual(2, job.Batches[2].Steps.Count);

			step = job.Batches[0].Steps[0];
			Assert.AreEqual(0, step.NodeIdx);
			Assert.AreEqual(JobStepState.Completed, step.State);
			Assert.AreEqual(JobStepOutcome.Failure, step.Outcome);

			step = job.Batches[1].Steps[0];
			Assert.AreEqual(0, step.NodeIdx);
			Assert.AreEqual(JobStepState.Completed, step.State);
			Assert.AreEqual(JobStepOutcome.Failure, step.Outcome);

			step = job.Batches[2].Steps[0];
			Assert.AreEqual(0, step.NodeIdx);
			Assert.AreEqual(JobStepState.Ready, step.State);
			Assert.AreEqual(JobStepOutcome.Success, step.Outcome);

			step = job.Batches[2].Steps[1];
			Assert.AreEqual(1, step.NodeIdx);
			Assert.AreEqual(JobStepState.Waiting, step.State);
			Assert.AreEqual(JobStepOutcome.Success, step.Outcome);
		}

		[TestMethod]
		public async Task RetryStepDifferentGroupAsync()
		{
			Mock<ITemplate> templateMock = new Mock<ITemplate>(MockBehavior.Strict);
			templateMock.SetupGet(x => x.InitialAgentType).Returns((string?)null);

			List<NewGroup> newGroups = new List<NewGroup>();

			NewGroup group = AddGroup(newGroups);
			AddNode(group, "Step 1", null);

			NewGroup group2 = AddGroup(newGroups);
			AddNode(group2, "Step 2", new[] { "Step 1" });

			IGraph graph = await GraphCollection.AppendAsync(null, newGroups, null, null);

			CreateJobOptions options = new CreateJobOptions();
			options.Arguments.Add("-Target=Step 2");

			IJob job = await JobCollection.AddAsync(JobId.GenerateNewId(), new StreamId("ue4-main"), new TemplateId("test-build"), ContentHash.SHA1("hello"), graph, "Test job", 123, 123, options);

			// Fail the first step
			job = await StartBatch(job, graph, 0);
			job = await RunStep(job, graph, 0, 0, JobStepOutcome.Failure);
			job = Deref(await JobCollection.TryUpdateBatchAsync(job, graph, job.Batches[0].Id, null, JobStepBatchState.Complete, null));

			Assert.AreEqual(2, job.Batches.Count);
			Assert.AreEqual(1, job.Batches[0].Steps.Count);
			Assert.AreEqual(1, job.Batches[1].Steps.Count);

			IJobStep step = job.Batches[0].Steps[0];
			Assert.AreEqual(JobStepState.Completed, step.State);
			Assert.AreEqual(JobStepOutcome.Failure, step.Outcome);

			step = job.Batches[1].Steps[0];
			Assert.AreEqual(JobStepState.Skipped, step.State);
			Assert.AreEqual(JobStepOutcome.Failure, step.Outcome);

			// Retry the failed step
			job = Deref(await JobCollection.TryUpdateStepAsync(job, graph, job.Batches[0].Id, job.Batches[0].Steps[0].Id, newRetryByUserId: UserId.Anonymous));
			Assert.AreEqual(3, job.Batches.Count);
			Assert.AreEqual(1, job.Batches[0].Steps.Count);
			Assert.AreEqual(1, job.Batches[1].Steps.Count);
			Assert.AreEqual(1, job.Batches[2].Steps.Count);

			step = job.Batches[0].Steps[0];
			Assert.AreEqual(0, job.Batches[0].GroupIdx);
			Assert.AreEqual(0, step.NodeIdx);
			Assert.AreEqual(JobStepState.Completed, step.State);
			Assert.AreEqual(JobStepOutcome.Failure, step.Outcome);

			step = job.Batches[1].Steps[0];
			Assert.AreEqual(0, job.Batches[1].GroupIdx);
			Assert.AreEqual(0, step.NodeIdx);
			Assert.AreEqual(JobStepState.Ready, step.State);
			Assert.AreEqual(JobStepOutcome.Success, step.Outcome);

			step = job.Batches[2].Steps[0];
			Assert.AreEqual(1, job.Batches[2].GroupIdx);
			Assert.AreEqual(0, step.NodeIdx);
			Assert.AreEqual(JobStepState.Waiting, step.State);
			Assert.AreEqual(JobStepOutcome.Success, step.Outcome);
		}

		[TestMethod]
		public async Task RetryDownstreamStepAsync()
		{
			Mock<ITemplate> templateMock = new Mock<ITemplate>(MockBehavior.Strict);
			templateMock.SetupGet(x => x.InitialAgentType).Returns((string?)null);

			List<NewGroup> newGroups = new List<NewGroup>();

			NewGroup group = AddGroup(newGroups);
			AddNode(group, "Step 1", null);
			AddNode(group, "Step 2", new[] { "Step 1" });
			AddNode(group, "Step 3", new[] { "Step 2" });

			IGraph graph = await GraphCollection.AppendAsync(null, newGroups, null, null);

			CreateJobOptions options = new CreateJobOptions();
			options.Arguments.Add("-Target=Step 3");

			IJob job = await JobCollection.AddAsync(JobId.GenerateNewId(), new StreamId("ue4-main"), new TemplateId("test-build"), ContentHash.SHA1("hello"), graph, "Test job", 123, 123, options);

			// Fail the first step
			job = await StartBatch(job, graph, 0);
			job = await RunStep(job, graph, 0, 0, JobStepOutcome.Failure);

			Assert.AreEqual(1, job.Batches.Count);
			Assert.AreEqual(3, job.Batches[0].Steps.Count);

			IJobStep step = job.Batches[0].Steps[0];
			Assert.AreEqual(0, step.NodeIdx);
			Assert.AreEqual(JobStepState.Completed, step.State);
			Assert.AreEqual(JobStepOutcome.Failure, step.Outcome);

			step = job.Batches[0].Steps[1];
			Assert.AreEqual(1, step.NodeIdx);
			Assert.AreEqual(JobStepState.Skipped, step.State);
			Assert.AreEqual(JobStepOutcome.Failure, step.Outcome);

			step = job.Batches[0].Steps[2];
			Assert.AreEqual(2, step.NodeIdx);
			Assert.AreEqual(JobStepState.Skipped, step.State);
			Assert.AreEqual(JobStepOutcome.Failure, step.Outcome);

			// Retry the last failed step. All the failed upstream steps should run again.
			job = Deref(await JobCollection.TryUpdateStepAsync(job, graph, job.Batches[0].Id, job.Batches[0].Steps[2].Id, newRetryByUserId: UserId.Anonymous));
			Assert.AreEqual(2, job.Batches.Count);
			Assert.AreEqual(1, job.Batches[0].Steps.Count);
			Assert.AreEqual(3, job.Batches[1].Steps.Count);

			step = job.Batches[0].Steps[0];
			Assert.AreEqual(0, step.NodeIdx);
			Assert.AreEqual(JobStepState.Completed, step.State);
			Assert.AreEqual(JobStepOutcome.Failure, step.Outcome);

			step = job.Batches[1].Steps[0];
			Assert.AreEqual(0, step.NodeIdx);
			Assert.AreEqual(JobStepState.Ready, step.State);
			Assert.AreEqual(JobStepOutcome.Success, step.Outcome);

			step = job.Batches[1].Steps[1];
			Assert.AreEqual(1, step.NodeIdx);
			Assert.AreEqual(JobStepState.Waiting, step.State);
			Assert.AreEqual(JobStepOutcome.Success, step.Outcome);

			step = job.Batches[1].Steps[2];
			Assert.AreEqual(2, step.NodeIdx);
			Assert.AreEqual(JobStepState.Waiting, step.State);
			Assert.AreEqual(JobStepOutcome.Success, step.Outcome);
		}

		[TestMethod]
		public async Task UpdateBatchesDuringJobAsync()
		{
			Mock<ITemplate> templateMock = new Mock<ITemplate>(MockBehavior.Strict);
			templateMock.SetupGet(x => x.InitialAgentType).Returns((string?)null);

			List<NewGroup> newGroups = new List<NewGroup>();

			NewGroup group = AddGroup(newGroups);
			AddNode(group, "Step 1", null);
			AddNode(group, "Step 2", new[] { "Step 1" });
			AddNode(group, "Step 3", new[] { "Step 2" });

			IGraph graph = await GraphCollection.AppendAsync(null, newGroups, null, null);

			CreateJobOptions options = new CreateJobOptions();
			options.Arguments.Add("-Target=Step 3");

			IJob job = await JobCollection.AddAsync(JobId.GenerateNewId(), new StreamId("ue4-main"), new TemplateId("test-build"), ContentHash.SHA1("hello"), graph, "Test job", 123, 123, options);

			// Pass the first step
			job = await StartBatch(job, graph, 0);
			job = await RunStep(job, graph, 0, 0, JobStepOutcome.Success);

			Assert.AreEqual(1, job.Batches.Count);
			Assert.AreEqual(3, job.Batches[0].Steps.Count);

			IJobStep step = job.Batches[0].Steps[0];
			Assert.AreEqual(0, step.NodeIdx);
			Assert.AreEqual(JobStepState.Completed, step.State);
			Assert.AreEqual(JobStepOutcome.Success, step.Outcome);

			step = job.Batches[0].Steps[1];
			Assert.AreEqual(1, step.NodeIdx);
			Assert.AreEqual(JobStepState.Ready, step.State);

			step = job.Batches[0].Steps[2];
			Assert.AreEqual(2, step.NodeIdx);
			Assert.AreEqual(JobStepState.Waiting, step.State);

			// Update the priority of the second step. This should trigger a refresh of the graph structure, but NOT cause any new steps to be created.
			job = Deref(await JobCollection.TryUpdateStepAsync(job, graph, job.Batches[0].Id, job.Batches[0].Steps[1].Id, newPriority: Priority.High));
			Assert.AreEqual(1, job.Batches.Count);
			Assert.AreEqual(3, job.Batches[0].Steps.Count);

			step = job.Batches[0].Steps[0];
			Assert.AreEqual(0, step.NodeIdx);
			Assert.AreEqual(JobStepState.Completed, step.State);

			step = job.Batches[0].Steps[1];
			Assert.AreEqual(1, step.NodeIdx);
			Assert.AreEqual(JobStepState.Ready, step.State);

			step = job.Batches[0].Steps[2];
			Assert.AreEqual(2, step.NodeIdx);
			Assert.AreEqual(JobStepState.Waiting, step.State);
		}

		[TestMethod]
		public async Task UnknownShelfAsync()
		{
			Mock<ITemplate> templateMock = new Mock<ITemplate>(MockBehavior.Strict);
			templateMock.SetupGet(x => x.InitialAgentType).Returns((string?)null);

			IGraph baseGraph = await GraphCollection.AddAsync(templateMock.Object, null);

			CreateJobOptions options = new CreateJobOptions();
			options.Arguments.Add("-Target=Step 1");
			options.Arguments.Add("-Target=Step 3");

			IJob job = await JobCollection.AddAsync(JobId.GenerateNewId(), new StreamId("ue4-main"), new TemplateId("test-build"), ContentHash.SHA1("hello"), baseGraph, "Test job", 123, 123, options);
			Assert.AreEqual(1, job.Batches.Count);

			job = await StartBatch(job, baseGraph, 0);
			job = Deref(await JobCollection.TryUpdateBatchAsync(job, baseGraph, job.Batches[0].Id, null, JobStepBatchState.Complete, JobStepBatchError.UnknownShelf));

			Assert.AreEqual(1, job.Batches.Count);
		}
	}
}
