// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using Horde.Server.Perforce;
using Horde.Server.Users;
using HordeCommon;

namespace Horde.Server.Jobs.Bisect
{
	/// <summary>
	/// Manages a collection of bisect tasks
	/// </summary>
	public interface IBisectTaskCollection
	{
		/// <summary>
		/// Creates a new bisect task
		/// </summary>
		/// <param name="job">First job to search back from</param>
		/// <param name="nodeName">Name of the node to search for</param>
		/// <param name="outcome">Outcome of the step to search for</param>
		/// <param name="ownerId">User that initiated the search</param>
		/// <param name="options">Options for the bisection</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The new bisect task instance</returns>
		public Task<IBisectTask> CreateAsync(IJob job, string nodeName, JobStepOutcome outcome, UserId ownerId, CreateBisectTaskOptions? options = null, CancellationToken cancellationToken = default);

		/// <summary>
		/// Finds all the active bisect tasks
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>List of bisect tasks</returns>
		public IAsyncEnumerable<IBisectTask> FindActiveAsync(CancellationToken cancellationToken = default);

		/// <summary>
		/// Finds all bisect tasks for the provided criteria
		/// </summary>
		/// <param name="jobId">jobId of initial bisection</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>List of bisect tasks</returns>
		public Task<IReadOnlyList<IBisectTask>> FindAsync(JobId? jobId = null, CancellationToken cancellationToken = default);

		/// <summary>
		/// Gets a bisect task by id
		/// </summary>
		/// <param name="bisectTaskId">Id of the bisect task</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public Task<IBisectTask?> GetAsync(BisectTaskId bisectTaskId, CancellationToken cancellationToken = default);

		/// <summary>
		/// Updates the state of a bisect task
		/// </summary>
		/// <param name="bisectTask">Task to update</param>
		/// <param name="options">Settings for updating the task</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public Task<IBisectTask?> TryUpdateAsync(IBisectTask bisectTask, UpdateBisectTaskOptions options, CancellationToken cancellationToken = default);
	}

	/// <summary>
	/// Settings for creating a new bisect task
	/// </summary>
	public class CreateBisectTaskOptions
	{
		/// <summary>
		/// Filter the changes considered for the bisection
		/// </summary>
		public IReadOnlyList<CommitTag>? CommitTags { get; set; }

		/// <summary>
		/// Changes to exclude from the bisection
		/// </summary>
		public IReadOnlyList<int>? IgnoreChanges { get; set; }

		/// <summary>
		/// Jobs to exclude from the bisection
		/// </summary>
		public IReadOnlyList<JobId>? IgnoreJobs { get; set; }
	}

	/// <summary>
	/// Settings for updating a bisect task
	/// </summary>
	public class UpdateBisectTaskOptions
	{
		/// <summary>
		/// First job to bisect from
		/// </summary>
		public (JobId JobId, int Change)? CurrentJob { get; set; }

		/// <summary>
		/// New state for the task
		/// </summary>
		public BisectTaskState? State { get; set; }

		/// <summary>
		/// Changes to include in the bisection
		/// </summary>
		public IReadOnlyList<int>? IncludeChanges { get; set; }

		/// <summary>
		/// Changes to exclude from the bisection
		/// </summary>
		public IReadOnlyList<int>? ExcludeChanges { get; set; }

		/// <summary>
		/// Jobs to include in the bisection
		/// </summary>
		public IReadOnlyList<JobId>? IncludeJobs { get; set; }

		/// <summary>
		/// Jobs to exclude from the bisection
		/// </summary>
		public IReadOnlyList<JobId>? ExcludeJobs { get; set; }
	}
}
