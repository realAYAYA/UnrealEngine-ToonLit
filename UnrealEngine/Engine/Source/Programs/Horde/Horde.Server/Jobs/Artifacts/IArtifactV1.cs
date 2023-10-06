// Copyright Epic Games, Inc. All Rights Reserved.

using Horde.Server.Utilities;
using MongoDB.Bson;

namespace Horde.Server.Jobs.Artifacts
{
	/// <summary>
	/// Information about an artifact
	/// </summary>
	public interface IArtifactV1
	{
		/// <summary>
		/// Identifier for the Artifact. Randomly generated.
		/// </summary>
		public ObjectId Id { get; }

		/// <summary>
		/// Unique id of the job containing this artifact
		/// </summary>
		public JobId JobId { get; }

		/// <summary>
		/// Unique id of the job containing this artifact
		/// </summary>
		public string Name { get; }

		/// <summary>
		/// Unique id of the step containing this artifact
		/// </summary>
		public SubResourceId? StepId { get; }

		/// <summary>
		/// Total size of the file
		/// </summary>
		public long Length { get; }

		/// <summary>
		/// Type of artifact
		/// </summary>
		public string MimeType { get; }
	}
}
