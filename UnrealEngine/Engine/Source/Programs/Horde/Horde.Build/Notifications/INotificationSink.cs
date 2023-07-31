// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using Horde.Build.Devices;
using Horde.Build.Issues;
using Horde.Build.Jobs;
using Horde.Build.Jobs.Graphs;
using Horde.Build.Logs;
using Horde.Build.Streams;
using Horde.Build.Users;
using HordeCommon;

namespace Horde.Build.Notifications
{
	/// <summary>
	/// Implements a notification method
	/// </summary>
	public interface INotificationSink
	{
		/// <summary>
		/// Send notifications that a job has been scheduled
		/// </summary>
		/// <param name="notifications">List of notifications to send</param>
		/// <returns>Async task</returns>
		Task NotifyJobScheduledAsync(List<JobScheduledNotification> notifications);
		
		/// <summary>
		/// Send notifications that a job has completed
		/// </summary>
		/// <param name="jobStream"></param>
		/// <param name="job">The job containing the step</param>
		/// <param name="graph"></param>
		/// <param name="outcome"></param>
		/// <returns>Async task</returns>
		Task NotifyJobCompleteAsync(IStream jobStream, IJob job, IGraph graph, LabelOutcome outcome);

		/// <summary>
		/// Send notifications that a job has completed
		/// </summary>
		/// <param name="user">User to notify</param>
		/// <param name="jobStream"></param>
		/// <param name="job">The job containing the step</param>
		/// <param name="graph"></param>
		/// <param name="outcome"></param>
		/// <returns>Async task</returns>
		Task NotifyJobCompleteAsync(IUser user, IStream jobStream, IJob job, IGraph graph, LabelOutcome outcome);

		/// <summary>
		/// Send notifications that a job step has completed
		/// </summary>
		/// <param name="user">User to notify</param>
		/// <param name="jobStream">Stream containing the job</param>
		/// <param name="job">The job containing the step</param>
		/// <param name="batch">Unique id of the batch</param>
		/// <param name="step">The step id</param>
		/// <param name="node">Corresponding node for the step</param>
		/// <param name="jobStepEventData"></param>
		/// <returns>Async task</returns>
		Task NotifyJobStepCompleteAsync(IUser user, IStream jobStream, IJob job, IJobStepBatch batch, IJobStep step, INode node, List<ILogEventData> jobStepEventData);

		/// <summary>
		/// Send notifications that a job step has completed
		/// </summary>
		/// <param name="user">User to notify</param>
		/// <param name="job">The job containing the step</param>
		/// <param name="stream"></param>
		/// <param name="label"></param>
		/// <param name="labelIdx"></param>
		/// <param name="outcome"></param>
		/// <param name="stepData"></param>
		/// <returns>Async task</returns>
		Task NotifyLabelCompleteAsync(IUser user, IJob job, IStream stream, ILabel label, int labelIdx, LabelOutcome outcome, List<(string, JobStepOutcome, Uri)> stepData);

		#region Issues

		/// <summary>
		/// Send notifications that a new issue has been created or assigned
		/// </summary>
		/// <param name="issue"></param>
		/// <returns></returns>
		Task NotifyIssueUpdatedAsync(IIssue issue);

		/// <summary>
		/// Notify the status of issues in a stream
		/// </summary>
		/// <param name="report"></param>
		/// <returns></returns>
		Task SendIssueReportAsync(IssueReportGroup report);

		#endregion

		/// <summary>
		/// Notification that a stream has failed to update
		/// </summary>
		/// <param name="errorMessage"></param>
		/// <param name="fileName"></param>
		/// <param name="change"></param>
		/// <param name="author"></param>
		/// <param name="description"></param>
		/// <returns></returns>
		Task NotifyConfigUpdateFailureAsync(string errorMessage, string fileName, int? change = null, IUser? author = null, string? description = null);

		/// <summary>
		/// Notification for device service
		/// </summary>
		/// <param name="message"></param>
		/// <param name="device"></param>
		/// <param name="pool"></param>
		/// <param name="stream"></param>
		/// <param name="job"></param>
		/// <param name="step"></param>
		/// <param name="node"></param>
		/// <param name="user"></param>
		/// <returns></returns>
		Task NotifyDeviceServiceAsync(string message, IDevice? device = null, IDevicePool? pool = null, IStream? stream = null, IJob? job = null, IJobStep? step = null, INode? node = null, IUser? user = null);
    }
}

