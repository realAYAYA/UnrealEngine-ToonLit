// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using EpicGames.Horde.Artifacts;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Streams;
using Horde.Server.Acls;

namespace Horde.Server.Artifacts
{
	/// <summary>
	/// Information about an artifact
	/// </summary>
	public interface IArtifact
	{
		/// <summary>
		/// Identifier for the Artifact. Randomly generated.
		/// </summary>
		public ArtifactId Id { get; }

		/// <summary>
		/// Name of the artifact
		/// </summary>
		public ArtifactName Name { get; }

		/// <summary>
		/// Type of artifact
		/// </summary>
		public ArtifactType Type { get; }

		/// <summary>
		/// Description for the artifact
		/// </summary>
		public string? Description { get; }

		/// <summary>
		/// Identifier for the stream that produced the artifact
		/// </summary>
		public StreamId StreamId { get; }

		/// <summary>
		/// Change that the artifact corresponds to
		/// </summary>
		public int Change { get; }

		/// <summary>
		/// Keys used to collate artifacts
		/// </summary>
		public IReadOnlyList<string> Keys { get; }

		/// <summary>
		/// Metadata for the artifact
		/// </summary>
		public IReadOnlyList<string> Metadata { get; }

		/// <summary>
		/// Storage namespace containing the data
		/// </summary>
		public NamespaceId NamespaceId { get; }

		/// <summary>
		/// Name of the ref containing the root data object
		/// </summary>
		public RefName RefName { get; }

		/// <summary>
		/// Time at which the artifact was created
		/// </summary>
		public DateTime CreatedAtUtc { get; }

		/// <summary>
		/// Permissions scope for this object
		/// </summary>
		public AclScopeName AclScope { get; }
	}
}
