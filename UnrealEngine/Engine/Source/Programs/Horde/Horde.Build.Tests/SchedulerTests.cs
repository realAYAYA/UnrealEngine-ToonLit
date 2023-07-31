// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Horde.Build.Jobs;
using Horde.Build.Logs;
using Horde.Build.Server;
using Horde.Build.Projects;
using Horde.Build.Streams;
using Horde.Build.Jobs.Templates;
using Horde.Build.Jobs.Schedules;
using Horde.Build.Users;
using Horde.Build.Utilities;
using HordeCommon;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Horde.Build.Jobs.Graphs;
using Horde.Build.Perforce;

namespace Horde.Build.Tests
{
	using JobId = ObjectId<IJob>;
	using LogId = ObjectId<ILogFile>;
	using ProjectId = StringId<IProject>;
	using StreamId = StringId<IStream>;
	using TemplateRefId = StringId<TemplateRef>;

	[TestClass]
    public class SchedulerTests : TestSetup
	{
		ProjectId ProjectId { get; } = new ProjectId("ue5");
		StreamId StreamId { get; } = new StreamId("ue5-main");
		TemplateRefId TemplateRefId { get; } = new TemplateRefId("template1");

		readonly ITemplate _template;
		readonly HashSet<JobId> _initialJobIds;

		public SchedulerTests()
		{
			IUser bob = UserCollection.FindOrAddUserByLoginAsync("Bob").Result;

			IProject ? project = ProjectService.Collection.AddOrUpdateAsync(ProjectId, "", "", 0, new ProjectConfig { Name = "UE5" }).Result;
			Assert.IsNotNull(project);

			_template = TemplateCollection.AddAsync("Test template").Result;

			_initialJobIds = new HashSet<JobId>(JobCollection.FindAsync().Result.Select(x => x.Id));

			PerforceService.Changes.Clear();
			PerforceService.AddChange("//UE5/Main", 100, bob, "", new[] { "code.cpp" });
			PerforceService.AddChange("//UE5/Main", 101, bob, "", new[] { "content.uasset" });
			PerforceService.AddChange("//UE5/Main", 102, bob, "", new[] { "content.uasset" });
		}

		async Task<IStream> SetScheduleAsync(CreateScheduleRequest schedule)
		{
			await ScheduleService.ResetAsync();

			IStream? stream = await StreamService.GetStreamAsync(StreamId);

			StreamConfig config = new StreamConfig();
			config.Name = "//UE5/Main";
			config.Tabs.Add(new CreateJobsTabRequest { Title = "foo", Templates = new List<TemplateRefId> { TemplateRefId } });
			config.Templates.Add(new TemplateRefConfig { Id = TemplateRefId, Name = "Test", Schedule = schedule });

			return (await CreateOrReplaceStreamAsync(StreamId, stream, ProjectId, config))!; 
		}

		public async Task<List<IJob>> FileTestHelperAsync(params string[] files)
		{
			IUser bob = await UserCollection.FindOrAddUserByLoginAsync("Bob", "Bob");

			PerforceService.Changes.Clear();
			PerforceService.AddChange("//UE5/Main", 100, bob, "", new[] { "code.cpp" });
			PerforceService.AddChange("//UE5/Main", 101, bob, "", new[] { "content.uasset" });
			PerforceService.AddChange("//UE5/Main", 102, bob, "", new[] { "content.uasset" });
			PerforceService.AddChange("//UE5/Main", 103, bob, "", new[] { "foo/code.cpp" });
			PerforceService.AddChange("//UE5/Main", 104, bob, "", new[] { "bar/code.cpp" });
			PerforceService.AddChange("//UE5/Main", 105, bob, "", new[] { "foo/bar/content.uasset" });
			PerforceService.AddChange("//UE5/Main", 106, bob, "", new[] { "bar/foo/content.uasset" });

			DateTime startTime = new DateTime(2021, 1, 1, 12, 0, 0, DateTimeKind.Local); // Friday Jan 1, 2021 
			Clock.UtcNow = startTime;

			CreateScheduleRequest schedule = new CreateScheduleRequest();
			schedule.Enabled = true;
			schedule.MaxChanges = 10;
			schedule.Patterns.Add(new CreateSchedulePatternRequest { Interval = 1 });
			schedule.Files = files.ToList();
			await SetScheduleAsync(schedule);

			await Clock.AdvanceAsync(TimeSpan.FromHours(1.25));
			await ScheduleService.TickForTestingAsync();

			return await GetNewJobs();
		}

		[TestMethod]
		public async Task FileTest1Async()
		{
			List<IJob> jobs = await FileTestHelperAsync("....cpp");
			Assert.AreEqual(3, jobs.Count);
			Assert.AreEqual(100, jobs[0].Change);
			Assert.AreEqual(103, jobs[1].Change);
			Assert.AreEqual(104, jobs[2].Change);
		}

		[TestMethod]
		public async Task FileTest2Async()
		{
			List<IJob> jobs = await FileTestHelperAsync("/foo/...");
			Assert.AreEqual(2, jobs.Count);
			Assert.AreEqual(103, jobs[0].Change);
			Assert.AreEqual(105, jobs[1].Change);
		}

		[TestMethod]
		public async Task FileTest3Async()
		{
			List<IJob> jobs = await FileTestHelperAsync("....uasset", "-/bar/...");
			Assert.AreEqual(3, jobs.Count);
			Assert.AreEqual(101, jobs[0].Change);
			Assert.AreEqual(102, jobs[1].Change);
			Assert.AreEqual(105, jobs[2].Change);
		}

		[TestMethod]
		public void DayScheduleTest()
		{
			DateTime startTime = new DateTime(2021, 1, 1, 12, 0, 0, DateTimeKind.Utc); // Friday Jan 1, 2021 
			Clock.UtcNow = startTime;

			Schedule schedule = new Schedule(Clock.UtcNow, requireSubmittedChange: false);
			schedule.Patterns.Add(new SchedulePattern(new List<DayOfWeek> { DayOfWeek.Friday, DayOfWeek.Sunday }, 13 * 60, null, null));

			DateTime? nextTime = schedule.GetNextTriggerTimeUtc(startTime, TimeZoneInfo.Utc);
			Assert.AreEqual(startTime + TimeSpan.FromHours(1.0), nextTime!.Value);

			nextTime = schedule.GetNextTriggerTimeUtc(nextTime.Value, TimeZoneInfo.Utc);
			Assert.AreEqual(startTime + TimeSpan.FromHours(1.0 + 24.0 * 2.0), nextTime!.Value);

			nextTime = schedule.GetNextTriggerTimeUtc(nextTime.Value, TimeZoneInfo.Utc);
			Assert.AreEqual(startTime + TimeSpan.FromHours(1.0 + 24.0 * 7.0), nextTime!.Value);

			nextTime = schedule.GetNextTriggerTimeUtc(nextTime.Value, TimeZoneInfo.Utc);
			Assert.AreEqual(startTime + TimeSpan.FromHours(1.0 + 24.0 * 9.0), nextTime!.Value);
		}

		[TestMethod]
		public void MulticheduleTest()
		{
			DateTime startTime = new DateTime(2021, 1, 1, 12, 0, 0, DateTimeKind.Utc); // Friday Jan 1, 2021 
			Clock.UtcNow = startTime;

			Schedule schedule = new Schedule(Clock.UtcNow, requireSubmittedChange: false);
			schedule.Patterns.Add(new SchedulePattern(null, 13 * 60, 14 * 60, 15));

			DateTime? nextTime = schedule.GetNextTriggerTimeUtc(startTime, TimeZoneInfo.Utc);
			Assert.AreEqual(startTime + TimeSpan.FromHours(1.0), nextTime!.Value);

			nextTime = schedule.GetNextTriggerTimeUtc(nextTime.Value, TimeZoneInfo.Utc);
			Assert.AreEqual(startTime + TimeSpan.FromHours(1.25), nextTime!.Value);

			nextTime = schedule.GetNextTriggerTimeUtc(nextTime.Value, TimeZoneInfo.Utc);
			Assert.AreEqual(startTime + TimeSpan.FromHours(1.5), nextTime!.Value);

			nextTime = schedule.GetNextTriggerTimeUtc(nextTime.Value, TimeZoneInfo.Utc);
			Assert.AreEqual(startTime + TimeSpan.FromHours(1.75), nextTime!.Value);

			nextTime = schedule.GetNextTriggerTimeUtc(nextTime.Value, TimeZoneInfo.Utc);
			Assert.AreEqual(startTime + TimeSpan.FromHours(2.0), nextTime!.Value);

			nextTime = schedule.GetNextTriggerTimeUtc(nextTime.Value, TimeZoneInfo.Utc);
			Assert.AreEqual(startTime + TimeSpan.FromHours(1.0 + 24.0), nextTime!.Value);
		}

		[TestMethod]
		public void MultiPatternTest()
		{
			DateTime startTime = new DateTime(2021, 1, 1, 0, 0, 0, DateTimeKind.Utc); // Friday Jan 1, 2021 
			Clock.UtcNow = startTime;

			Schedule schedule = new Schedule(Clock.UtcNow, requireSubmittedChange: false);
			schedule.Patterns.Add(new SchedulePattern(null, 11 * 60, 0, 0));
			schedule.Patterns.Add(new SchedulePattern(null, 19 * 60, 0, 0));

			DateTime? nextTime = schedule.GetNextTriggerTimeUtc(startTime, TimeZoneInfo.Utc);
			Assert.AreEqual(startTime + TimeSpan.FromHours(11), nextTime!.Value);

			nextTime = schedule.GetNextTriggerTimeUtc(nextTime.Value, TimeZoneInfo.Utc);
			Assert.AreEqual(startTime + TimeSpan.FromHours(19), nextTime!.Value);
		}

		[TestMethod]
		public async Task NoSubmittedChangeTestAsync()
		{
			DateTime startTime = new DateTime(2021, 1, 1, 12, 0, 0, DateTimeKind.Local); // Friday Jan 1, 2021 
			Clock.UtcNow = startTime;

			CreateScheduleRequest schedule = new CreateScheduleRequest();
			schedule.Enabled = true;
			schedule.Patterns.Add(new CreateSchedulePatternRequest { MinTime = 13 * 60, MaxTime = 14 * 60, Interval = 15 });
//			Schedule.LastTriggerTime = StartTime;
			await SetScheduleAsync(schedule);

			// Initial tick
			await ScheduleService.TickForTestingAsync();

			List<IJob> jobs1 = await GetNewJobs();
			Assert.AreEqual(0, jobs1.Count);

			// Trigger a job
			await Clock.AdvanceAsync(TimeSpan.FromHours(1.25));
			await ScheduleService.TickForTestingAsync();

			List<IJob> jobs2 = await GetNewJobs();
			Assert.AreEqual(1, jobs2.Count);
			Assert.AreEqual(102, jobs2[0].Change);
			Assert.AreEqual(100, jobs2[0].CodeChange);
		}

		[TestMethod]
		public async Task RequireSubmittedChangeTestAsync()
		{
			DateTime startTime = new DateTime(2021, 1, 1, 12, 0, 0, DateTimeKind.Local); // Friday Jan 1, 2021 
			Clock.UtcNow = startTime;

			CreateScheduleRequest schedule = new CreateScheduleRequest();
			schedule.Enabled = true;
			schedule.Patterns.Add(new CreateSchedulePatternRequest { MinTime = 13 * 60, MaxTime = 14 * 60, Interval = 15 });
			IStream stream = await SetScheduleAsync(schedule);

			// Initial tick
			await ScheduleService.TickForTestingAsync();

			List<IJob> jobs1 = await GetNewJobs();
			Assert.AreEqual(0, jobs1.Count);

			// Trigger a job
			await Clock.AdvanceAsync(TimeSpan.FromHours(1.25));
			await ScheduleService.TickForTestingAsync();

			List<IJob> jobs2 = await GetNewJobs();
			Assert.AreEqual(1, jobs2.Count);
			Assert.AreEqual(102, jobs2[0].Change);
			Assert.AreEqual(100, jobs2[0].CodeChange);

			IStream stream2 = (await StreamCollection.GetAsync(stream.Id))!;
			Schedule schedule2 = stream2.Templates.First().Value.Schedule!;
			Assert.AreEqual(102, schedule2.LastTriggerChange);
			Assert.AreEqual(Clock.UtcNow, schedule2.LastTriggerTime);

			// Trigger another job
			await Clock.AdvanceAsync(TimeSpan.FromHours(0.5));
			await ScheduleService.TickForTestingAsync();

			List<IJob> jobs3 = await GetNewJobs();
			Assert.AreEqual(0, jobs3.Count);
		}

		[TestMethod]
		public async Task MultipleJobsTestAsync()
		{
			DateTime startTime = new DateTime(2021, 1, 1, 12, 0, 0, DateTimeKind.Local); // Friday Jan 1, 2021 
			Clock.UtcNow = startTime;

			CreateScheduleRequest schedule = new CreateScheduleRequest();
			schedule.Enabled = true;
			schedule.Patterns.Add(new CreateSchedulePatternRequest { MinTime = 13 * 60, MaxTime = 14 * 60, Interval = 15 });
			schedule.MaxChanges = 2;
			schedule.Filter = new List<ChangeContentFlags> { ChangeContentFlags.ContainsCode };
			await SetScheduleAsync(schedule);

			// Initial tick
			await ScheduleService.TickForTestingAsync();

			List<IJob> jobs1 = await GetNewJobs();
			Assert.AreEqual(0, jobs1.Count);

			// Trigger some jobs
			IUser bob = await UserCollection.FindOrAddUserByLoginAsync("Bob");
			PerforceService.AddChange("//UE5/Main", 103, bob, "", new string[] { "foo.cpp" });
			PerforceService.AddChange("//UE5/Main", 104, bob, "", new string[] { "foo.cpp" });
			PerforceService.AddChange("//UE5/Main", 105, bob, "", new string[] { "foo.uasset" });
			PerforceService.AddChange("//UE5/Main", 106, bob, "", new string[] { "foo.cpp" });

			await Clock.AdvanceAsync(TimeSpan.FromHours(1.25));
			await ScheduleService.TickForTestingAsync();

			List<IJob> jobs2 = await GetNewJobs();
			Assert.AreEqual(2, jobs2.Count);
			Assert.AreEqual(104, jobs2[0].Change);
			Assert.AreEqual(104, jobs2[0].CodeChange);
			Assert.AreEqual(106, jobs2[1].Change);
			Assert.AreEqual(106, jobs2[1].CodeChange);
		}

		[TestMethod]
		public async Task SkipCiTestAsync()
		{
			DateTime startTime = new DateTime(2021, 1, 1, 12, 0, 0, DateTimeKind.Local); // Friday Jan 1, 2021 
			Clock.UtcNow = startTime;

			CreateScheduleRequest schedule = new CreateScheduleRequest();
			schedule.Enabled = true;
			schedule.Patterns.Add(new CreateSchedulePatternRequest { MinTime = 13 * 60, MaxTime = 14 * 60, Interval = 15 });
			schedule.MaxChanges = 2;
			schedule.Filter = new List<ChangeContentFlags> { ChangeContentFlags.ContainsCode };
			await SetScheduleAsync(schedule);

			// Initial tick
			await ScheduleService.TickForTestingAsync();

			List<IJob> jobs1 = await GetNewJobs();
			Assert.AreEqual(0, jobs1.Count);

			// Trigger some jobs
			IUser bob = await UserCollection.FindOrAddUserByLoginAsync("Bob");
			PerforceService.AddChange("//UE5/Main", 103, bob, "", new string[] { "foo.cpp" });
			PerforceService.AddChange("//UE5/Main", 104, bob, "Don't build this change!\n#skipci", new string[] { "foo.cpp" });
			PerforceService.AddChange("//UE5/Main", 105, bob, "", new string[] { "foo.uasset" });
			PerforceService.AddChange("//UE5/Main", 106, bob, "", new string[] { "foo.cpp" });

			await Clock.AdvanceAsync(TimeSpan.FromHours(1.25));
			await ScheduleService.TickForTestingAsync();

			List<IJob> jobs2 = await GetNewJobs();
			Assert.AreEqual(2, jobs2.Count);
			Assert.AreEqual(103, jobs2[0].Change);
			Assert.AreEqual(103, jobs2[0].CodeChange);
			Assert.AreEqual(106, jobs2[1].Change);
			Assert.AreEqual(106, jobs2[1].CodeChange);
		}

		[TestMethod]
		public async Task MaxActiveTestAsync()
		{
			DateTime startTime = new DateTime(2021, 1, 1, 12, 0, 0, DateTimeKind.Local); // Friday Jan 1, 2021 
			Clock.UtcNow = startTime;

			CreateScheduleRequest schedule = new CreateScheduleRequest();
			schedule.Enabled = true;
			schedule.RequireSubmittedChange = false;
			schedule.Patterns.Add(new CreateSchedulePatternRequest { MinTime = 13 * 60, MaxTime = 14 * 60, Interval = 15 });
			schedule.MaxActive = 1;
			await SetScheduleAsync(schedule);

			// Trigger a job
			await Clock.AdvanceAsync(TimeSpan.FromHours(1.25));
			await ScheduleService.TickForTestingAsync();

			List<IJob> jobs2 = await GetNewJobs();
			Assert.AreEqual(1, jobs2.Count);
			Assert.AreEqual(102, jobs2[0].Change);
			Assert.AreEqual(100, jobs2[0].CodeChange);

			// Test that another job does not trigger
			await Clock.AdvanceAsync(TimeSpan.FromHours(0.5));
			await ScheduleService.TickForTestingAsync();

			List<IJob> jobs3 = await GetNewJobs();
			Assert.AreEqual(0, jobs3.Count);

			// Mark the original job as complete
			await JobService.UpdateJobAsync(jobs2[0], abortedByUserId: KnownUsers.System);

			// Test that another job does not trigger
			await Clock.AdvanceAsync(TimeSpan.FromHours(0.5));
			await ScheduleService.TickForTestingAsync();

			List<IJob> jobs4 = await GetNewJobs();
			Assert.AreEqual(1, jobs4.Count);
			Assert.AreEqual(102, jobs4[0].Change);
			Assert.AreEqual(100, jobs4[0].CodeChange);
		}

		[TestMethod]
		public async Task CreateNewChangeTestAsync()
		{
			DateTime startTime = new DateTime(2021, 1, 1, 12, 0, 0, DateTimeKind.Local); // Friday Jan 1, 2021 
			Clock.UtcNow = startTime;

			CreateScheduleRequest schedule = new CreateScheduleRequest();
			schedule.Enabled = true;
			schedule.Patterns.Add(new CreateSchedulePatternRequest { MinTime = 13 * 60, MaxTime = 14 * 60, Interval = 15 });
			/*IStream stream = */await SetScheduleAsync(schedule);

			// Trigger a job
			await Clock.AdvanceAsync(TimeSpan.FromHours(1.25));
			await ScheduleService.TickForTestingAsync();

			List<IJob> jobs2 = await GetNewJobs();
			Assert.AreEqual(1, jobs2.Count);
			Assert.AreEqual(102, jobs2[0].Change);
			Assert.AreEqual(100, jobs2[0].CodeChange);

			// Check another job does not trigger due to the change above
			await Clock.AdvanceAsync(TimeSpan.FromHours(1.25));
			await ScheduleService.TickForTestingAsync();

			List<IJob> jobs3 = await GetNewJobs();
			Assert.AreEqual(0, jobs3.Count);
		}

		[TestMethod]
		public async Task GateTestAsync()
		{
			DateTime startTime = new DateTime(2021, 1, 1, 12, 0, 0, DateTimeKind.Local); // Friday Jan 1, 2021 
			Clock.UtcNow = startTime;

			// Create two templates, the second dependent on the first
			ITemplate? newTemplate1 = await TemplateCollection.AddAsync("Test template 1");
			//TemplateRef newTemplateRef1 = new TemplateRef(newTemplate1);
			TemplateRefId newTemplateRefId1 = new TemplateRefId("new-template-1");

			ITemplate? newTemplate2 = await TemplateCollection.AddAsync("Test template 2");
			TemplateRef newTemplateRef2 = new TemplateRef(newTemplate2);
			newTemplateRef2.Schedule = new Schedule(Clock.UtcNow);
			newTemplateRef2.Schedule.Gate = new ScheduleGate(newTemplateRefId1, "TriggerNext");
			newTemplateRef2.Schedule.Patterns.Add(new SchedulePattern(null, 0, null, 10));
			newTemplateRef2.Schedule.LastTriggerTime = startTime;
			TemplateRefId newTemplateRefId2 = new TemplateRefId("new-template-2");

			IStream? stream = await StreamService.GetStreamAsync(StreamId);

			StreamConfig config = new StreamConfig();
			config.Name = "//UE5/Main";
			config.Tabs.Add(new CreateJobsTabRequest { Title = "foo", Templates = new List<TemplateRefId> { newTemplateRefId1, newTemplateRefId2 } });

			stream = (await CreateOrReplaceStreamAsync(StreamId, stream, ProjectId, config))!;

			// Create the TriggerNext step and mark it as complete
			IGraph graphA = await GraphCollection.AddAsync(newTemplate1);
			NewGroup groupA = new NewGroup("win", new List<NewNode> { new NewNode("TriggerNext") });
			graphA = await GraphCollection.AppendAsync(graphA, new List<NewGroup> { groupA });

			// Tick the schedule and make sure it doesn't trigger
			await ScheduleService.TickForTestingAsync();
			List<IJob> jobs2 = await GetNewJobs();
			Assert.AreEqual(0, jobs2.Count);

			// Create a job and fail it
			IJob job1 = await JobService.CreateJobAsync(null, stream, newTemplateRefId1, _template.Id, graphA, "Hello", 1234, 1233, 999, null, null, null, null, null, null, null, null, true, true, null, null, new List<string> { "-Target=TriggerNext" });
			SubResourceId batchId1 = job1.Batches[0].Id;
			SubResourceId stepId1 = job1.Batches[0].Steps[0].Id;
			job1 = Deref(await JobService.UpdateBatchAsync(job1, batchId1, LogId.GenerateNewId(), JobStepBatchState.Running));
			job1 = Deref(await JobService.UpdateStepAsync(job1, batchId1, stepId1, JobStepState.Completed, JobStepOutcome.Failure));
			job1 = Deref(await JobService.UpdateBatchAsync(job1, batchId1, LogId.GenerateNewId(), JobStepBatchState.Complete));
			Assert.IsNotNull(job1);
			await GetNewJobs();

			// Tick the schedule and make sure it doesn't trigger
			await Clock.AdvanceAsync(TimeSpan.FromMinutes(30.0));
			await ScheduleService.TickForTestingAsync();
			List<IJob> jobs3 = await GetNewJobs();
			Assert.AreEqual(0, jobs3.Count);

			// Create a job and make it succeed
			IJob job2 = await JobService.CreateJobAsync(null, stream, newTemplateRefId1, _template.Id, graphA, "Hello", 1234, 1233, 999, null, null, null, null, null, null, null, null, true, true, null, null, new List<string> { "-Target=TriggerNext" });
			SubResourceId batchId2 = job2.Batches[0].Id;
			SubResourceId stepId2 = job2.Batches[0].Steps[0].Id;
			job2 = Deref(await JobService.UpdateBatchAsync(job2, batchId2, LogId.GenerateNewId(), JobStepBatchState.Running));
			job2 = Deref(await JobService.UpdateStepAsync(job2, batchId2, stepId2, JobStepState.Completed, JobStepOutcome.Success));
			Assert.IsNotNull(job2);

			// Tick the schedule and make sure it does trigger
			await ScheduleService.TickForTestingAsync();
			List<IJob> jobs4 = await GetNewJobs();
			Assert.AreEqual(1, jobs4.Count);
			Assert.AreEqual(1234, jobs4[0].Change);
			Assert.AreEqual(1233, jobs4[0].CodeChange);
		}

		[TestMethod]
		public async Task GateTest2Async()
		{
			IUser bob = await UserCollection.FindOrAddUserByLoginAsync("Bob");

			DateTime startTime = new DateTime(2021, 1, 1, 12, 0, 0, DateTimeKind.Local); // Friday Jan 1, 2021 
			Clock.UtcNow = startTime;

			PerforceService.Changes.Clear();
			PerforceService.AddChange("//UE5/Main", 1230, bob, "", new[] { "code.cpp" });
			PerforceService.AddChange("//UE5/Main", 1231, bob, "", new[] { "content.uasset" });
			PerforceService.AddChange("//UE5/Main", 1232, bob, "", new[] { "content.uasset" });
			PerforceService.AddChange("//UE5/Main", 1233, bob, "", new[] { "code.cpp" });

			// Create two templates, the second dependent on the first
			TemplateRefId newTemplateRefId1 = new TemplateRefId("new-template-1");
			TemplateRefConfig newTemplate1 = new TemplateRefConfig();
			newTemplate1.Id = newTemplateRefId1;

			TemplateRefId newTemplateRefId2 = new TemplateRefId("new-template-2");
			TemplateRefConfig newTemplate2 = new TemplateRefConfig();
			newTemplate2.Id = newTemplateRefId2;
			newTemplate2.Name = "Test template 2";
			newTemplate2.Schedule = new CreateScheduleRequest();
			newTemplate2.Schedule.MaxChanges = 4;
			newTemplate2.Schedule.Filter = new List<ChangeContentFlags> { ChangeContentFlags.ContainsCode };
			newTemplate2.Schedule.Gate = new CreateScheduleGateRequest { TemplateId = newTemplateRefId1.ToString(), Target = "TriggerNext" };
			newTemplate2.Schedule.Patterns.Add(new CreateSchedulePatternRequest { Interval = 10 });// (null, 0, null, 10));
//			NewTemplate2.Schedule.LastTriggerTime = StartTime;

			IStream? stream = await StreamService.GetStreamAsync(StreamId);

			StreamConfig config = new StreamConfig();
			config.Name = "//UE5/Main";
			config.Tabs.Add(new CreateJobsTabRequest { Title = "foo", Templates = new List<TemplateRefId> { newTemplateRefId1, newTemplateRefId2 } });
			config.Templates.Add(newTemplate1);
			config.Templates.Add(newTemplate2);

			stream = (await CreateOrReplaceStreamAsync(StreamId, stream, ProjectId, config))!;

			ITemplate template1 = (await TemplateCollection.GetAsync(stream.Templates[newTemplateRefId1].Hash))!;
			Assert.IsNotNull(template1);
			ITemplate template2 = (await TemplateCollection.GetAsync(stream.Templates[newTemplateRefId2].Hash))!;
			Assert.IsNotNull(template2);

			// Create the graph
			IGraph graphA = await GraphCollection.AddAsync(template1);
			NewGroup groupA = new NewGroup("win", new List<NewNode> { new NewNode("TriggerNext") });
			graphA = await GraphCollection.AppendAsync(graphA, new List<NewGroup> { groupA });

			// Create successful jobs for all the changes we added above
			for (int change = 1230; change <= 1233; change++)
			{
				int codeChange = (change < 1233) ? 1230 : 1233;

				IJob job1 = await JobService.CreateJobAsync(null, stream, newTemplateRefId1, _template.Id, graphA, "Hello", change, codeChange, null, null, null, null, null, null, false, null, null, true, true, null, null, new List<string> { "-Target=TriggerNext" });
				for (int batchIdx = 0; batchIdx < job1.Batches.Count; batchIdx++)
				{
					SubResourceId batchId1 = job1.Batches[batchIdx].Id;
					job1 = Deref(await JobService.UpdateBatchAsync(job1, batchId1, LogId.GenerateNewId(), JobStepBatchState.Running));
					for (int stepIdx = 0; stepIdx < job1.Batches[batchIdx].Steps.Count; stepIdx++)
					{
						SubResourceId stepId1 = job1.Batches[batchIdx].Steps[stepIdx].Id;
						job1 = Deref(await JobService.UpdateStepAsync(job1, batchId1, stepId1, JobStepState.Completed, JobStepOutcome.Success, newLogId: LogId.GenerateNewId()));
					}
					job1 = Deref(await JobService.UpdateBatchAsync(job1, batchId1, LogId.GenerateNewId(), JobStepBatchState.Complete));
				}
			}
			await GetNewJobs();

			// Tick the schedule and make sure it doesn't trigger
			await Clock.AdvanceAsync(TimeSpan.FromMinutes(30.0));
			await ScheduleService.TriggerAsync(StreamId, newTemplateRefId2, Clock.UtcNow, default);
			List<IJob> jobs3 = await GetNewJobs();
			Assert.AreEqual(2, jobs3.Count);
			Assert.AreEqual(1230, jobs3[0].Change);
			Assert.AreEqual(1233, jobs3[1].Change);
		}

		[TestMethod]
		public async Task UpdateConfigAsync()
		{
			DateTime startTime = new DateTime(2021, 1, 1, 12, 0, 0, DateTimeKind.Local); // Friday Jan 1, 2021 
			Clock.UtcNow = startTime;

			CreateScheduleRequest schedule = new CreateScheduleRequest();
			schedule.Enabled = true;
			schedule.RequireSubmittedChange = false;
			schedule.Patterns.Add(new CreateSchedulePatternRequest { MinTime = 13 * 60, MaxTime = 14 * 60, Interval = 15 });
			schedule.MaxActive = 2;
			await SetScheduleAsync(schedule);

			await Clock.AdvanceAsync(TimeSpan.FromHours(1.25));
			await ScheduleService.TickForTestingAsync();

			List<IJob> jobs1 = await GetNewJobs();
			Assert.AreEqual(1, jobs1.Count);
			Assert.AreEqual(102, jobs1[0].Change);
			Assert.AreEqual(100, jobs1[0].CodeChange);

			// Make sure the job is registered
			IStream? stream1 = await StreamService.GetStreamAsync(StreamId);
			TemplateRef templateRef1 = stream1!.Templates.First().Value;
			Assert.AreEqual(1, templateRef1.Schedule!.ActiveJobs.Count);
			Assert.AreEqual(jobs1[0].Id, templateRef1.Schedule!.ActiveJobs[0]);

			// Test that another job does not trigger
			await SetScheduleAsync(schedule);

			// Make sure the job is still registered
			IStream? stream2 = await StreamService.GetStreamAsync(StreamId);
			TemplateRef templateRef2 = stream2!.Templates.First().Value;
			Assert.AreEqual(1, templateRef2.Schedule!.ActiveJobs.Count);
			Assert.AreEqual(jobs1[0].Id, templateRef2.Schedule!.ActiveJobs[0]);
		}

		[TestMethod]
		public async Task StreamPausing()
		{
			DateTime startTime = new DateTime(2021, 1, 1, 12, 0, 0, DateTimeKind.Utc); // Friday Jan 1, 2021 
			Clock.UtcNow = startTime;

			CreateScheduleRequest schedule = new CreateScheduleRequest();
			schedule.Enabled = true;
			schedule.Patterns.Add(new CreateSchedulePatternRequest { MinTime = 13 * 60, MaxTime = 14 * 60, Interval = 15 });
			IStream stream = await SetScheduleAsync(schedule);

			IStreamCollection streamCollection = ServiceProvider.GetRequiredService<IStreamCollection>();
			await streamCollection.TryUpdatePauseStateAsync(stream, newPausedUntil: startTime.AddHours(5), newPauseComment: "testing");

			// Try trigger a job. No job should be scheduled as the stream is paused
			await Clock.AdvanceAsync(TimeSpan.FromHours(1.25));
			await ScheduleService.TickForTestingAsync();
			List<IJob> jobs2 = await GetNewJobs();
			Assert.AreEqual(0, jobs2.Count);

			// Advance time beyond the pause period. A build should now trigger
			await Clock.AdvanceAsync(TimeSpan.FromHours(5.25));
			await ScheduleService.TickForTestingAsync();

			List<IJob> jobs3 = await GetNewJobs();
			Assert.AreEqual(1, jobs3.Count);
			Assert.AreEqual(102, jobs3[0].Change);
			Assert.AreEqual(100, jobs3[0].CodeChange);
		}

		async Task<List<IJob>> GetNewJobs()
		{
			List<IJob> jobs = await JobCollection.FindAsync();
			jobs.RemoveAll(x => _initialJobIds.Contains(x.Id));
			_initialJobIds.UnionWith(jobs.Select(x => x.Id));
			return jobs.OrderBy(x => x.Change).ToList();
		}
	}
}