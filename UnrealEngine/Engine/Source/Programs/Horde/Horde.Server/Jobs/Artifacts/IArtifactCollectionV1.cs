// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Threading.Tasks;
using Horde.Server.Utilities;
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
		/// <returns>The new log file document</returns>
		Task<IArtifactV1> CreateArtifactAsync(JobId jobId, SubResourceId? stepId, string name, string mimeType, System.IO.Stream data);

		/// <summary>
		/// Gets all the available artifacts for a job
		/// </summary>
		/// <param name="jobId">Unique id of the job to query</param>
		/// <param name="stepId">Unique id of the Step to query</param>
		/// <param name="name">Name of the artifact</param>
		/// <returns>List of artifact documents</returns>
		Task<List<IArtifactV1>> GetArtifactsAsync(JobId? jobId, SubResourceId? stepId, string? name);

		/// <summary>
		/// Gets a specific list of artifacts based on id
		/// </summary>
		/// <param name="artifactIds">The list of artifact Ids</param>
		/// <returns>List of artifact documents</returns>
		Task<List<IArtifactV1>> GetArtifactsAsync(IEnumerable<ObjectId> artifactIds);

		/// <summary>
		/// Gets an artifact by ID
		/// </summary>
		/// <param name="artifactId">Unique id of the artifact</param>
		/// <returns>The artifact document</returns>
		Task<IArtifactV1?> GetArtifactAsync(ObjectId artifactId);

		/// <summary>
		/// Updates an artifact
		/// </summary>
		/// <param name="artifact">The artifact</param>
		/// <param name="newMimeType">New mime type</param>
		/// <param name="newData">New data</param>
		/// <returns>Async task</returns>
		Task<bool> UpdateArtifactAsync(IArtifactV1? artifact, string newMimeType, System.IO.Stream newData);

		/// <summary>
		/// gets artifact data
		/// </summary>
		/// <param name="artifact">The artifact</param>
		/// <returns>The chunk data</returns>
		Task<System.IO.Stream> OpenArtifactReadStreamAsync(IArtifactV1 artifact);
	}
}
