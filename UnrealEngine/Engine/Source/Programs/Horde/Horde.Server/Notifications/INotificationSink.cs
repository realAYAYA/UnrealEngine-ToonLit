// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using Horde.Server.Agents;
using Horde.Server.Devices;
using Horde.Server.Issues;
using Horde.Server.Jobs;
using Horde.Server.Jobs.Graphs;
using Horde.Server.Logs;
using Horde.Server.Streams;
using Horde.Server.Users;
using HordeCommon;

namespace Horde.Server.Notifications
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
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Async task</returns>
		Task NotifyJobScheduledAsync(List<JobScheduledNotification> notifications, CancellationToken cancellationToken);

		/// <summary>
		/// Send notifications that a job has completed
		/// </summary>
		/// <param name="job">The job containing the step</param>
		/// <param name="graph"></param>
		/// <param name="outcome"></param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Async task</returns>
		Task NotifyJobCompleteAsync(IJob job, IGraph graph, LabelOutcome outcome, CancellationToken cancellationToken);

		/// <summary>
		/// Send notifications that a job has completed
		/// </summary>
		/// <param name="user">User to notify</param>
		/// <param name="job">The job containing the step</param>
		/// <param name="graph"></param>
		/// <param name="outcome"></param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Async task</returns>
		Task NotifyJobCompleteAsync(IUser user, IJob job, IGraph graph, LabelOutcome outcome, CancellationToken cancellationToken);

		/// <summary>
		/// Send notifications that a job step has completed
		/// </summary>
		/// <param name="user">User to notify</param>
		/// <param name="job">The job containing the step</param>
		/// <param name="batch">Unique id of the batch</param>
		/// <param name="step">The step id</param>
		/// <param name="node">Corresponding node for the step</param>
		/// <param name="jobStepEventData"></param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Async task</returns>
		Task NotifyJobStepCompleteAsync(IUser user, IJob job, IJobStepBatch batch, IJobStep step, INode node, List<ILogEventData> jobStepEventData, CancellationToken cancellationToken);

		/// <summary>
		/// Send notifications that a job step has completed
		/// </summary>
		/// <param name="user">User to notify</param>
		/// <param name="job">The job containing the step</param>
		/// <param name="label"></param>
		/// <param name="labelIdx"></param>
		/// <param name="outcome"></param>
		/// <param name="stepData"></param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Async task</returns>
		Task NotifyLabelCompleteAsync(IUser user, IJob job, ILabel label, int labelIdx, LabelOutcome outcome, List<(string, JobStepOutcome, Uri)> stepData, CancellationToken cancellationToken);

		#region Issues

		/// <summary>
		/// Send notifications that a new issue has been created or assigned
		/// </summary>
		/// <param name="issue"></param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		Task NotifyIssueUpdatedAsync(IIssue issue, CancellationToken cancellationToken);

		/// <summary>
		/// Notify the status of issues in a stream
		/// </summary>
		/// <param name="report"></param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		Task SendIssueReportAsync(IssueReportGroup report, CancellationToken cancellationToken);

		#endregion

		/// <summary>
		/// Notification that the configuration state has changed
		/// </summary>
		/// <param name="ex">Exception during update. Null if the update completed successfully.</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		Task NotifyConfigUpdateAsync(Exception? ex, CancellationToken cancellationToken);

		/// <summary>
		/// Notification that a stream has failed to update
		/// </summary>
		/// <param name="errorMessage"></param>
		/// <param name="fileName"></param>
		/// <param name="change"></param>
		/// <param name="author"></param>
		/// <param name="description"></param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		Task NotifyConfigUpdateFailureAsync(string errorMessage, string fileName, int? change = null, IUser? author = null, string? description = null, CancellationToken cancellationToken = default);

		/// <summary>
		/// Notification for device service
		/// </summary>
		/// <param name="message"></param>
		/// <param name="device"></param>
		/// <param name="pool"></param>
		/// <param name="streamConfig"></param>
		/// <param name="job"></param>
		/// <param name="step"></param>
		/// <param name="node"></param>
		/// <param name="user"></param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		Task NotifyDeviceServiceAsync(string message, IDevice? device = null, IDevicePool? pool = null, StreamConfig? streamConfig = null, IJob? job = null, IJobStep? step = null, INode? node = null, IUser? user = null, CancellationToken cancellationToken = default);

		/// <summary>
		/// Notify the status of devices
		/// </summary>
		/// <param name="report"></param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		Task SendDeviceIssueReportAsync(DeviceIssueReport report, CancellationToken cancellationToken);

		/// <summary>
		/// Notify the status of agents in the farm
		/// </summary>
		Task SendAgentReportAsync(AgentReport report, CancellationToken cancellationToken);
	}
}

