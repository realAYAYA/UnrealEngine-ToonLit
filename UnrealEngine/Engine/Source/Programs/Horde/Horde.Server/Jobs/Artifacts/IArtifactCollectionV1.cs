// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Jobs;
using MongoDB.Bson;

namespace Horde.Server.Jobs.Artifacts
{
	/// <summary>
	/// Interface for a collection of artifacts
	/// </summary>
	public interface IArtifactCollectionV1
	{
		/// <summary>
		/// Creates a new artifact
		/// </summary>
		/// <param name="jobId">Unique id of the job that owns this artifact</param>
		/// <param name="stepId">Optional Step id</param>
		/// <param name="name">Name of artifact</param>
		/// <param name="mimeType">Type of artifact</param>
		/// <param name="data">The data to write</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The new log file document</returns>
		Task<IArtifactV1> CreateArtifactAsync(JobId jobId, JobStepId? stepId, string name, string mimeType, System.IO.Stream data, CancellationToken cancellationToken = default);

		/// <summary>
		/// Gets all the available artifacts for a job
		/// </summary>
		/// <param name="jobId">Unique id of the job to query</param>
		/// <param name="stepId">Unique id of the Step to query</param>
		/// <param name="name">Name of the artifact</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>List of artifact documents</returns>
		Task<IReadOnlyList<IArtifactV1>> GetArtifactsAsync(JobId? jobId, JobStepId? stepId, string? name, CancellationToken cancellationToken = default);

		/// <summary>
		/// Gets a specific list of artifacts based on id
		/// </summary>
		/// <param name="artifactIds">The list of artifact Ids</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>List of artifact documents</returns>
		Task<IReadOnlyList<IArtifactV1>> GetArtifactsAsync(IEnumerable<ObjectId> artifactIds, CancellationToken cancellationToken = default);

		/// <summary>
		/// Gets an artifact by ID
		/// </summary>
		/// <param name="artifactId">Unique id of the artifact</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The artifact document</returns>
		Task<IArtifactV1?> GetArtifactAsync(ObjectId artifactId, CancellationToken cancellationToken = default);

		/// <summary>
		/// Updates an artifact
		/// </summary>
		/// <param name="artifact">The artifact</param>
		/// <param name="newMimeType">New mime type</param>
		/// <param name="newData">New data</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Async task</returns>
		Task<bool> UpdateArtifactAsync(IArtifactV1? artifact, string newMimeType, System.IO.Stream newData, CancellationToken cancellationToken = default);

		/// <summary>
		/// gets artifact data
		/// </summary>
		/// <param name="artifact">The artifact</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The chunk data</returns>
		Task<System.IO.Stream> OpenArtifactReadStreamAsync(IArtifactV1 artifact, CancellationToken cancellationToken = default);
	}
}
