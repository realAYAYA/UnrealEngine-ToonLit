// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using Horde.Build.Agents.Pools;
using Horde.Build.Devices;
using Horde.Build.Issues;
using Horde.Build.Jobs;
using Horde.Build.Jobs.Graphs;
using Horde.Build.Notifications;
using Horde.Build.Users;
using Horde.Build.Streams;
using Horde.Build.Utilities;
using HordeCommon;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Moq;
using Horde.Build.Logs;

namespace Horde.Build.Tests
{
	using JobId = ObjectId<IJob>;
	using StreamId = StringId<IStream>;

	public class FakeNotificationSink : INotificationSink
	{
		public List<JobScheduledNotification> JobScheduledNotifications { get; } = new();
		public int JobScheduledCallCount { get; set; }
		
		public Task NotifyJobScheduledAsync(List<JobScheduledNotification> notifications)
		{
			JobScheduledNotifications.AddRange(notifications);
			JobScheduledCallCount++;
			return Task.CompletedTask;
		}

		public Task NotifyJobCompleteAsync(IStream jobStream, IJob job, IGraph graph, LabelOutcome outcome) { throw new NotImplementedException(); }
		public Task NotifyJobCompleteAsync(IUser user, IStream jobStream, IJob job, IGraph graph, LabelOutcome outcome) { throw new NotImplementedException(); }
		public Task NotifyJobStepCompleteAsync(IUser user, IStream jobStream, IJob job, IJobStepBatch batch, IJobStep step, INode node, List<ILogEventData> jobStepEventData) { throw new NotImplementedException(); }
		public Task NotifyLabelCompleteAsync(IUser user, IJob job, IStream stream, ILabel label, int labelIdx, LabelOutcome outcome, List<(string, JobStepOutcome, Uri)> stepData) { throw new NotImplementedException(); }
		public Task NotifyIssueUpdatedAsync(IIssue issue) { throw new NotImplementedException(); }
		public Task NotifyConfigUpdateFailureAsync(string errorMessage, string fileName, int? change = null, IUser? author = null, string? description = null) { throw new NotImplementedException(); }
		public Task NotifyDeviceServiceAsync(string message, IDevice? device = null, IDevicePool? pool = null, IStream? stream = null, IJob? job = null, IJobStep? step = null, INode? node = null, IUser? user = null) { throw new NotImplementedException(); }
		public Task SendIssueReportAsync(IssueReportGroup report) => throw new NotImplementedException();
	}

	[TestClass]
	public class NotificationServiceTests : TestSetup
	{
		protected override void ConfigureServices(IServiceCollection services)
		{
			base.ConfigureServices(services);

			services.AddSingleton<FakeNotificationSink>();
			services.AddSingleton<INotificationSink>(sp => sp.GetRequiredService<FakeNotificationSink>());
		}

		public static IJob CreateJob(StreamId streamId, int change, string name, IGraph graph)
		{
			JobId jobId = new JobId("5ec16da1774cb4000107c2c1");

			List<IJobStepBatch> batches = new List<IJobStepBatch>();
			for (int groupIdx = 0; groupIdx < graph.Groups.Count; groupIdx++)
			{
				INodeGroup @group = graph.Groups[groupIdx];

				List<IJobStep> steps = new List<IJobStep>();
				for (int nodeIdx = 0; nodeIdx < @group.Nodes.Count; nodeIdx++)
				{
					SubResourceId stepId = new SubResourceId((ushort)((groupIdx * 100) + nodeIdx));

					Mock<IJobStep> step = new Mock<IJobStep>(MockBehavior.Strict);
					step.SetupGet(x => x.Id).Returns(stepId);
					step.SetupGet(x => x.NodeIdx).Returns(nodeIdx);

					steps.Add(step.Object);
				}

				SubResourceId batchId = new SubResourceId((ushort)(groupIdx * 100));

				Mock<IJobStepBatch> batch = new Mock<IJobStepBatch>(MockBehavior.Strict);
				batch.SetupGet(x => x.Id).Returns(batchId);
				batch.SetupGet(x => x.GroupIdx).Returns(groupIdx);
				batch.SetupGet(x => x.Steps).Returns(steps);
				batches.Add(batch.Object);
			}

			Mock<IJob> job = new Mock<IJob>(MockBehavior.Strict);
			job.SetupGet(x => x.Id).Returns(jobId);
			job.SetupGet(x => x.Name).Returns(name);
			job.SetupGet(x => x.StreamId).Returns(streamId);
			job.SetupGet(x => x.Change).Returns(change);
			job.SetupGet(x => x.Batches).Returns(batches);
			return job.Object;
		}
		
		[TestMethod]
		public async Task NotifyJobScheduled()
		{
			FakeNotificationSink fakeSink = ServiceProvider.GetRequiredService<FakeNotificationSink>();

			NotificationService service = (NotificationService)ServiceProvider.GetRequiredService<INotificationService>();
			await service._ticker.StartAsync();

			Fixture fixture = await CreateFixtureAsync();
			IPool pool = await PoolService.CreatePoolAsync("BogusPool", properties: new Dictionary<string, string>());

			Assert.AreEqual(0, fakeSink.JobScheduledNotifications.Count);
			service.NotifyJobScheduled(pool, false, fixture.Job1, fixture.Graph, SubResourceId.Random());
			service.NotifyJobScheduled(pool, false, fixture.Job2, fixture.Graph, SubResourceId.Random());
			
			// Currently no good way to wait for NotifyJobScheduled() to complete as the execution is completely async in background task (see ExecuteAsync)
			await Task.Delay(1000);
			await Clock.AdvanceAsync(service._notificationBatchInterval + TimeSpan.FromMinutes(5));
			Assert.AreEqual(2, fakeSink.JobScheduledNotifications.Count);
			Assert.AreEqual(1, fakeSink.JobScheduledCallCount);
		}

		//public void NotifyLabelUpdate(IJob Job, IReadOnlyList<(LabelState, LabelOutcome)> OldLabelStates, IReadOnlyList<(LabelState, LabelOutcome)> NewLabelStates)
		//{
		//	// If job has any label trigger IDs, send label complete notifications if needed
		//	if (Job.LabelIdxToTriggerId.Any())
		//	{
		//		EnqueueTask(() => SendAllLabelNotificationsAsync(Job, OldLabelStates, NewLabelStates));
		//	}
		//}

		//private async Task SendAllLabelNotificationsAsync(IJob Job, IReadOnlyList<(LabelState State, LabelOutcome Outcome)> OldLabelStates, IReadOnlyList<(LabelState, LabelOutcome)> NewLabelStates)
		//{
		//	IStream? Stream = await StreamService.GetStreamAsync(Job.StreamId);
		//	if (Stream == null)
		//	{
		//		return;
		//	}

		//	IGraph? Graph = await GraphCollection.GetAsync(Job.GraphHash);
		//	if (Graph == null)
		//	{
		//		return;
		//	}

		//	IReadOnlyDictionary<NodeRef, IJobStep> StepForNode = Job.GetStepForNodeMap();
		//	for (int LabelIdx = 0; LabelIdx < Graph.Labels.Count; ++LabelIdx)
		//	{
		//		(LabelState State, LabelOutcome Outcome) OldLabel = OldLabelStates[LabelIdx];
		//		(LabelState State, LabelOutcome Outcome) NewLabel = NewLabelStates[LabelIdx];
		//		if (OldLabel != NewLabel)
		//		{
		//			// If the state transitioned from Unspecified to Running, don't update unless the outcome also changed.
		//			if (OldLabel.State == LabelState.Unspecified && NewLabel.State == LabelState.Running && OldLabel.Outcome == NewLabel.Outcome)
		//			{
		//				continue;
		//			}

		//			// If the label isn't complete, don't report on outcome changing to success, this will be reported when the label state becomes complete.
		//			if (NewLabel.State != LabelState.Complete && NewLabel.Outcome == LabelOutcome.Success)
		//			{
		//				return;
		//			}

		//			bool bFireTrigger = NewLabel.State == LabelState.Complete;
		//			INotificationTrigger? Trigger = await GetNotificationTrigger(Job.LabelIdxToTriggerId[LabelIdx], bFireTrigger);
		//			if (Trigger == null)
		//			{
		//				continue;
		//			}

		//			await SendLabelUpdateNotificationsAsync(Job, Stream, Graph, StepForNode, Graph.Labels[LabelIdx], NewLabel.State, NewLabel.Outcome, Trigger);
		//		}
		//	}
		//}
	}
}
