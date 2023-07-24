// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Common;
using Horde.Build.Agents;
using Horde.Build.Agents.Pools;
using Horde.Build.Jobs;
using Horde.Build.Jobs.Graphs;
using Horde.Build.Jobs.Templates;
using Horde.Build.Logs;
using Horde.Build.Perforce;
using Horde.Build.Projects;
using Horde.Build.Server;
using Horde.Build.Streams;
using Horde.Build.Users;
using Horde.Build.Utilities;
using HordeCommon;
using Microsoft.CodeAnalysis;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Build.Tests
{
	using JobId = ObjectId<IJob>;
	using LogId = ObjectId<ILogFile>;
	using ProjectId = StringId<ProjectConfig>;
	using StreamId = StringId<IStream>;
	using TemplateId = StringId<ITemplateRef>;

	[TestClass]
	public class JobServiceTests : TestSetup
	{
		[TestMethod]
		public async Task TestChainedJobs()
		{
			ProjectId projectId = new ProjectId("ue5");
			StreamId streamId = new StreamId("ue5-main");
			TemplateId templateRefId1 = new TemplateId("template1");
			TemplateId templateRefId2 = new TemplateId("template2");

			StreamConfig streamConfig = new StreamConfig { Id = streamId };
			streamConfig.Templates.Add(new TemplateRefConfig { Id = templateRefId1, Name = "Test Template", ChainedJobs = new List<ChainedJobTemplateConfig> { new ChainedJobTemplateConfig { TemplateId = templateRefId2, Trigger = "Setup Build" } } });
			streamConfig.Templates.Add(new TemplateRefConfig { Id = templateRefId2, Name = "Test Template" });
			streamConfig.Tabs.Add(new JobsTabConfig { Title = "foo", Templates = new List<TemplateId> { templateRefId1, templateRefId2 } });

			ProjectConfig projectConfig = new ProjectConfig { Id = projectId };
			projectConfig.Streams.Add(streamConfig);

			GlobalConfig globalConfig = new GlobalConfig();
			globalConfig.Projects.Add(projectConfig);

			SetConfig(globalConfig);

			CreateJobOptions options = new CreateJobOptions();
			options.PreflightChange = 999;
			options.JobTriggers.AddRange(streamConfig.Templates[0].ChainedJobs!);

			ITemplate template = await TemplateCollection.GetOrAddAsync(streamConfig.Templates[0]);

			IGraph graph = await GraphCollection.AddAsync(template, null);

			IJob job = await JobService.CreateJobAsync(null, streamConfig, templateRefId1, template.Hash, graph, "Hello", 1234, 1233, options);
			Assert.AreEqual(1, job.ChainedJobs.Count);

			job = Deref(await JobService.UpdateBatchAsync(job, job.Batches[0].Id, streamConfig, LogId.GenerateNewId(), JobStepBatchState.Running));
			job = Deref(await JobService.UpdateStepAsync(job, job.Batches[0].Id, job.Batches[0].Steps[0].Id, streamConfig, JobStepState.Running));
			job = Deref(await JobService.UpdateStepAsync(job, job.Batches[0].Id, job.Batches[0].Steps[0].Id, streamConfig, JobStepState.Completed, JobStepOutcome.Success));

			Assert.IsNotNull(job.ChainedJobs[0].JobId);

			IJob? chainedJob = await JobCollection.GetAsync(job.ChainedJobs[0].JobId!.Value);
			Assert.IsNotNull(chainedJob);
			Assert.AreEqual(chainedJob!.Id, job!.ChainedJobs[0].JobId);

			Assert.AreEqual(chainedJob!.Change, job!.Change);
			Assert.AreEqual(chainedJob!.CodeChange, job!.CodeChange);
			Assert.AreEqual(chainedJob!.PreflightChange, job!.PreflightChange);
			Assert.AreEqual(chainedJob!.StartedByUserId, job!.StartedByUserId);
		}

		[TestMethod]
		public async Task ChangeQueryTestAsync()
		{
			StreamId streamId = new StreamId("ue5-main");
			StreamConfig streamConfig = new StreamConfig { Id = streamId };

			ProjectId projectId = new ProjectId("ue5");
			ProjectConfig projectConfig = new ProjectConfig { Id = projectId, Name = "UE4", Streams = new List<StreamConfig> { streamConfig } };

			SetConfig(new GlobalConfig { Projects = new List<ProjectConfig> { projectConfig } });

			IUser user = await UserCollection.FindOrAddUserByLoginAsync("Bob");

			PerforceService.AddChange(streamId, 1000, user, "", new[] { "Foo.cpp" });
			PerforceService.AddChange(streamId, 1001, user, "", new[] { "Bar.cpp" });
			PerforceService.AddChange(streamId, 1002, user, "", new[] { "Baz.cpp" });
			PerforceService.AddChange(streamId, 1003, user, "", new[] { "Foo.uasset" });
			PerforceService.AddChange(streamId, 1004, user, "", new[] { "Foo.uasset" });

			ICommitCollection commits = PerforceService.GetCommits(streamConfig);

			{
				int? change = await JobService.EvaluateChangeQueryAsync(streamId, new ChangeQueryConfig { CommitTag = CommitTag.Code }, null, commits, CancellationToken.None);
				Assert.AreEqual(1002, change);
			}
			{
				int? change = await JobService.EvaluateChangeQueryAsync(streamId, new ChangeQueryConfig { CommitTag = CommitTag.Content }, null, commits, CancellationToken.None);
				Assert.AreEqual(1004, change);
			}
			{
				int? change = await JobService.EvaluateChangeQueryAsync(streamId, new ChangeQueryConfig { CommitTag = CommitTag.Content, Condition = "tag.code == 1" }, new List<CommitTag> { CommitTag.Code }, commits, CancellationToken.None);
				Assert.AreEqual(1004, change);
			}
			{
				int? change = await JobService.EvaluateChangeQueryAsync(streamId, new ChangeQueryConfig { CommitTag = CommitTag.Content, Condition = "tag.content == 1" }, new List<CommitTag> { CommitTag.Code }, commits, CancellationToken.None);
				Assert.IsNull(change);
			}
		}

		[TestMethod]
		public async Task StopAnyDuplicateJobsByPreflight()
		{
			Fixture fixture = await CreateFixtureAsync();

			string[] args = {"-Target=bogus"};
			IJob orgJob = await CreatePreflightJob(fixture, "tpl-ref-1", "tpl-hash-1", "elvis", 1000, args);
			IJob newJob = await CreatePreflightJob(fixture, "tpl-ref-1", "tpl-hash-1", "elvis", 1000, args);
			IJob differentTplRef = await CreatePreflightJob(fixture, "tpl-ref-other", "tpl-hash-1", "elvis", 1000, args);
			IJob differentTplHash = await CreatePreflightJob(fixture, "tpl-ref-1", "tpl-hash-other", "elvis", 1000, args);
			IJob differentUserName = await CreatePreflightJob(fixture, "tpl-ref-1", "tpl-hash-1", "julia", 1000, args);
			IJob differentArgs = await CreatePreflightJob(fixture, "tpl-ref-1", "tpl-hash-1", "elvis", 1000, new []{"-Target=other"});
			
			orgJob = (await JobService.GetJobAsync(orgJob.Id))!;
			newJob = (await JobService.GetJobAsync(newJob.Id))!;
			differentTplRef = (await JobService.GetJobAsync(differentTplRef.Id))!;
			differentTplHash = (await JobService.GetJobAsync(differentTplHash.Id))!;
			differentUserName = (await JobService.GetJobAsync(differentUserName.Id))!;
			differentArgs = (await JobService.GetJobAsync(differentArgs.Id))!;
			
			Assert.AreEqual(KnownUsers.System, orgJob.AbortedByUserId);
			Assert.IsNull(newJob.AbortedByUserId);
			Assert.IsNull(differentTplRef.AbortedByUserId);
			Assert.IsNull(differentTplHash.AbortedByUserId);
			Assert.IsNull(differentUserName.AbortedByUserId);
			Assert.IsNull(differentArgs.AbortedByUserId);
		}

		private async Task<IJob> CreatePreflightJob(Fixture fixture, string templateRefId, string templateHash, string startedByUserName, int preflightChange, string[] arguments)
		{
			IUser user = await UserCollection.FindOrAddUserByLoginAsync(startedByUserName);

			CreateJobOptions options = new CreateJobOptions();
			options.PreflightChange = preflightChange;
			options.StartedByUserId = user.Id;
			options.Arguments.AddRange(arguments);

			return await JobService.CreateJobAsync(
				jobId: JobId.GenerateNewId(),
				streamConfig: fixture!.StreamConfig!,
				templateRefId: new TemplateId(templateRefId),
				templateHash: new ContentHash(Encoding.ASCII.GetBytes(templateHash)),
				graph: fixture!.Graph,
				name: "hello1",
				change: 1000001,
				codeChange: 1000002,
				options
			);
		}
		
		// Only test for cancelled preflights
		// [TestMethod]
		// public async Task StopAnyDuplicateJobsByChange()
		// {
		// 	TestSetup TestSetup = await GetTestSetup();
		// 	Fixture Fix = Fixture!;
		// 	IJob Job1 = Fix.Job1;
		// 	IJob Job2 = Fix.Job2;
		// 	
		// 	Assert.AreEqual(JobState.Waiting, Job1.GetState());
		// 	Assert.IsTrue(await JobService.UpdateBatchAsync(Job1, Job1.Batches[0].Id, ObjectId.GenerateNewId(), JobStepBatchState.Running));
		//
		// 	Job1 = (await JobService.GetJobAsync(Job1.Id))!;
		// 	Job2 = (await JobService.GetJobAsync(Job2.Id))!;
		// 	Assert.AreEqual(JobState.Running, Job1.GetState());
		// 	Assert.AreEqual(JobState.Complete, Job2.GetState());
		// 	
		// 	IJob Job3 = await JobService.CreateJobAsync(null, Fix.Stream!.Id, Fix.TemplateRefId1, Fix.Template.Id, Fix.Graph, "Hello", Job1.Change, Job1.CodeChange, null, "joe", null, null, true, true, null, null, new List<string>());
		// 	await DispatchService.TickOnlyForTestingAsync();
		// 	Job1 = (await JobService.GetJobAsync(Job1.Id))!;
		// 	Assert.AreEqual(JobState.Complete, Job1.GetState());
		// 	Assert.AreEqual(JobState.Waiting, Job3.GetState());
		// }

		[TestMethod]
		public async Task TestRunEarly()
		{
			StreamId streamId = new StreamId("ue5-main");
			StreamConfig streamConfig = new StreamConfig { Id = streamId };

			ProjectId projectId = new ProjectId("ue5");
			ProjectConfig projectConfig = new ProjectConfig { Id = projectId, Name = "UE5", Streams = new List<StreamConfig> { streamConfig } };

			SetConfig(new GlobalConfig { Projects = new List<ProjectConfig> { projectConfig } });

			// ----

			IAgent? agent = await AgentService.CreateAgentAsync("TestAgent", true, new List<StringId<IPool>> { new StringId<IPool>("win") });
			await AgentService.CreateSessionAsync(agent, AgentStatus.Ok, new List<string>(), new Dictionary<string, int>(), null);

			ITemplate template = await TemplateCollection.GetOrAddAsync(new TemplateConfig { Name = "Test template" });
			IGraph graph = await GraphCollection.AddAsync(template, null);

			NewGroup groupA = new NewGroup("win", new List<NewNode>());
			groupA.Nodes.Add(new NewNode("Compile"));

			NewGroup groupB = new NewGroup("win", new List<NewNode>());
			groupB.Nodes.Add(new NewNode("Cook", runEarly: true));
			groupB.Nodes.Add(new NewNode("Middle"));
			groupB.Nodes.Add(new NewNode("Pak", inputDependencies: new List<string> { "Compile", "Cook" }));

			graph = await GraphCollection.AppendAsync(graph, new List<NewGroup> { groupA, groupB });

			CreateJobOptions options = new CreateJobOptions();
			options.PreflightChange = 999;
			options.Arguments.Add("-Target=Pak");

			IJob job = await JobService.CreateJobAsync(null, streamConfig!, new TemplateId("temp"), new ContentHash(new byte[] { 1, 2, 3 }), graph, "Hello", 1234, 1233, options);

			job = Deref(await JobService.UpdateBatchAsync(job, job.Batches[0].Id, streamConfig, LogId.GenerateNewId(), JobStepBatchState.Running));
			job = Deref(await JobService.UpdateStepAsync(job, job.Batches[0].Id, job.Batches[0].Steps[0].Id, streamConfig, JobStepState.Running));
			job = Deref(await JobService.UpdateStepAsync(job, job.Batches[0].Id, job.Batches[0].Steps[0].Id, streamConfig, JobStepState.Completed, JobStepOutcome.Success));

			job = Deref(await JobService.UpdateBatchAsync(job, job.Batches[1].Id, streamConfig, LogId.GenerateNewId(), JobStepBatchState.Running));
			job = Deref(await JobService.UpdateStepAsync(job, job.Batches[1].Id, job.Batches[1].Steps[0].Id, streamConfig, JobStepState.Running));

			job = Deref(await JobService.UpdateBatchAsync(job, job.Batches[2].Id, streamConfig, LogId.GenerateNewId(), JobStepBatchState.Running));
			job = Deref(await JobService.UpdateStepAsync(job, job.Batches[2].Id, job.Batches[2].Steps[0].Id, streamConfig, JobStepState.Running));
			job = Deref(await JobService.UpdateStepAsync(job, job.Batches[2].Id, job.Batches[2].Steps[0].Id, streamConfig, JobStepState.Completed, JobStepOutcome.Success));

			Assert.AreEqual(JobStepState.Waiting, job.Batches[2].Steps[1].State);
		}
	}
}
