// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Security.Claims;
using System.Threading.Tasks;
using Horde.Build.Devices;
using Horde.Build.Jobs;
using Horde.Build.Jobs.Graphs;
using Horde.Build.Issues;
using Horde.Build.Users;
using MongoDB.Bson;
using Horde.Build.Streams;

namespace Horde.Build.Notifications
{
	/// <summary>
	/// Marker interface for (serializable) notifications
	/// </summary>
	[SuppressMessage("Design", "CA1040:Avoid empty interfaces", Justification = "Marker interface for generic type handling")]
	public interface INotification { }

	/// <summary>
	/// Notification for job scheduled events
	/// </summary>
	public class JobScheduledNotification : INotification
	{
		/// <summary>Job ID</summary>
		public string JobId { get; }
		
		/// <summary>Job name</summary>
		public string JobName { get; }
		
		/// <summary>Pool name job got scheduled in</summary>
		public string PoolName { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="jobId"></param>
		/// <param name="jobName"></param>
		/// <param name="poolName"></param>
		public JobScheduledNotification(string jobId, string jobName, string poolName)
		{
			JobId = jobId;
			JobName = jobName;
			PoolName = poolName;
		}
	}
	
	/// <summary>
	/// Interface for the notification service
	/// </summary>
	public interface INotificationService
	{
		/// <summary>
		/// Updates a subscription to a trigger
		/// </summary>
		/// <param name="triggerId"></param>
		/// <param name="user"></param>
		/// <param name="email">Whether to receive email notifications</param>
		/// <param name="slack">Whether to receive Slack notifications</param>
		/// <returns></returns>
		Task<bool> UpdateSubscriptionsAsync(ObjectId triggerId, ClaimsPrincipal user, bool? email, bool? slack);

		/// <summary>
		/// Gets the current subscriptions for a user
		/// </summary>
		/// <param name="triggerId"></param>
		/// <param name="user"></param>
		/// <returns>Subscriptions for that user</returns>
		Task<INotificationSubscription?> GetSubscriptionsAsync(ObjectId triggerId, ClaimsPrincipal user);

		/// <summary>
		/// Notify all subscribers that a job step has finished
		/// </summary>
		/// <param name="job">The job containing the step that has finished</param>
		/// <param name="graph"></param>
		/// <param name="batchId">The batch id</param>
		/// <param name="stepId">The step id</param>
		/// <returns>Async task</returns>
		void NotifyJobStepComplete(IJob job, IGraph graph, SubResourceId batchId, SubResourceId stepId);
		
		/// <summary>
		/// Notify all subscribers that a job step's outcome has changed
		/// </summary>
		/// <param name="job">The job containing the step that has changed</param>
		/// <param name="oldLabelStates">The old label states for the job</param>
		/// <param name="newLabelStates">The new label states for the job</param>
		/// <returns>Async task</returns>
		void NotifyLabelUpdate(IJob job, IReadOnlyList<(LabelState, LabelOutcome)> oldLabelStates, IReadOnlyList<(LabelState, LabelOutcome)> newLabelStates);

		/// <summary>
		/// Notify slack channel about a stream update failure
		/// </summary>
		/// <param name="errorMessage">Error message passed back</param>
		/// <param name="fileName"></param>
		/// <param name="change">Latest change number of the file</param>
		/// <param name="author">Author of the change, if found from p4 service</param>
		/// <param name="description">Description of the change, if found from p4 service</param>
		void NotifyConfigUpdateFailure(string errorMessage, string fileName, int? change = null, IUser? author = null, string? description = null);

		/// <summary>
		/// Sends a notification to a user regarding a build health issue.
		/// </summary>
		/// <param name="issue">The new issue that was created</param>
		void NotifyIssueUpdated(IIssue issue);

		/// <summary>
		/// Send a device service notification
		/// </summary>
		/// <param name="message">The message to send</param>
		/// <param name="device">The device</param>
		/// <param name="pool">The pool</param>
		/// <param name="stream"></param>
		/// <param name="job"></param>
		/// <param name="step"></param>
		/// <param name="node"></param>
		/// <param name="user"></param>
		void NotifyDeviceService(string message, IDevice? device = null, IDevicePool? pool = null, IStream? stream = null, IJob? job = null, IJobStep? step = null, INode? node = null, IUser? user = null);

		/// <summary>
		/// Post a notification for the open issues in a stream
		/// </summary>
		/// <param name="report">The report data to send</param>
		Task SendIssueReportAsync(IssueReportGroup report);
	}
}
