// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Artifacts;
using EpicGames.Horde.Streams;
using Horde.Server.Acls;

namespace Horde.Server.Artifacts
{
	/// <summary>
	/// Interface for a collection of artifacts
	/// </summary>
	public interface IArtifactCollection
	{
		/// <summary>
		/// Creates a new artifact
		/// </summary>
		/// <param name="name">Name of the artifact</param>
		/// <param name="type">Type identifier for the artifact</param>
		/// <param name="description">Description for the artifact</param>
		/// <param name="streamId">Stream that the artifact was built from</param>
		/// <param name="change">Change number that the artifact was built from</param>
		/// <param name="keys">Keys for the artifact</param>
		/// <param name="metadata">Metadata for the artifact</param>
		/// <param name="scopeName">Inherited scope used for permissions</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The new log file document</returns>
		Task<IArtifact> AddAsync(ArtifactName name, ArtifactType type, string? description, StreamId streamId, int change, IEnumerable<string> keys, IEnumerable<string> metadata, AclScopeName scopeName, CancellationToken cancellationToken = default);

		/// <summary>
		/// Deletes artifacts
		/// </summary>
		/// <param name="ids">Ids to search for</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Sequence of artifacts</returns>
		Task DeleteAsync(IEnumerable<ArtifactId> ids, CancellationToken cancellationToken = default);

		/// <summary>
		/// Finds artifacts which are ready for expiry
		/// </summary>
		/// <param name="type">Type of artifacts to find</param>
		/// <param name="expireAtUtc">Number of artifacts to keep</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Sequence of artifacts</returns>
		IAsyncEnumerable<IEnumerable<IArtifact>> FindExpiredAsync(ArtifactType type, DateTime? expireAtUtc, CancellationToken cancellationToken = default);

		/// <summary>
		/// Finds artifacts with the given keys.
		/// </summary>
		/// <param name="streamId">Stream to find artifacts for</param>
		/// <param name="minChange">Minimum changelist number for the artifacts</param>
		/// <param name="maxChange">Maximum changelist number for the artifacts</param>
		/// <param name="name">Name of the artifact to search for</param>
		/// <param name="type">The artifact type</param>
		/// <param name="keys">Set of keys, all of which must all be present on any returned artifacts</param>
		/// <param name="maxResults">Maximum number of results to return</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Sequence of artifacts. Ordered by descending CL order, then by descending order in which they were created.</returns>
		IAsyncEnumerable<IArtifact> FindAsync(StreamId? streamId = null, int? minChange = null, int? maxChange = null, ArtifactName? name = null, ArtifactType? type = null, IEnumerable<string>? keys = null, int maxResults = 100, CancellationToken cancellationToken = default);

		/// <summary>
		/// Gets an artifact by ID
		/// </summary>
		/// <param name="artifactId">Unique id of the artifact</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The artifact document</returns>
		Task<IArtifact?> GetAsync(ArtifactId artifactId, CancellationToken cancellationToken = default);
	}

	/// <summary>
	/// Extension methods for <see cref="IArtifactCollection"/>
	/// </summary>
	public static class ArtifactCollectionExtensions
	{
		/// <inheritdoc cref="IArtifactCollection.DeleteAsync(IEnumerable{ArtifactId}, CancellationToken)"/>>
		public static Task DeleteAsync(this IArtifactCollection artifactCollection, ArtifactId artifactId, CancellationToken cancellationToken = default)
		{
			return artifactCollection.DeleteAsync(new[] { artifactId }, cancellationToken);
		}
	}
}
