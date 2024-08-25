// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Streams;

#pragma warning disable CA2227

namespace EpicGames.Horde.Artifacts
{
	/// <summary>
	/// Creates a new artifact
	/// </summary>
	/// <param name="Name">Name of the artifact</param>
	/// <param name="Type">Additional search keys tagged on the artifact</param>
	/// <param name="Description">Description for the artifact</param>
	/// <param name="StreamId">Stream to create the artifact for</param>
	/// <param name="Change">Change number for the artifact</param>
	/// <param name="Keys">Keys used to identify the artifact</param>
	/// <param name="Metadata">Metadata for the artifact</param>
	public record CreateArtifactRequest(ArtifactName Name, ArtifactType Type, string? Description, StreamId? StreamId, int? Change, List<string> Keys, List<string> Metadata);

	/// <summary>
	/// Information about a created artifact
	/// </summary>
	/// <param name="ArtifactId">Identifier for the new artifact</param>
	/// <param name="NamespaceId">Namespace that should be written to with artifact data</param>
	/// <param name="RefName">Ref to write to</param>
	/// <param name="PrevRefName">Ref for the artifact at the changelist prior to this one. Can be used to deduplicate against.</param>
	/// <param name="Token">Token which can be used to upload blobs for the artifact, and read blobs from the previous artifact</param>
	public record CreateArtifactResponse(ArtifactId ArtifactId, NamespaceId NamespaceId, RefName RefName, RefName? PrevRefName, string Token);

	/// <summary>
	/// Type of data to download for an artifact
	/// </summary>
	public enum DownloadArtifactFormat
	{
		/// <summary>
		/// Download as a zip file
		/// </summary>
		Zip,

		/// <summary>
		/// Download as a UGS link
		/// </summary>
		Ugs
	}

	/// <summary>
	/// Describes an artifact
	/// </summary>
	public class GetArtifactResponse
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
		/// Description for this artifact
		/// </summary>
		public string? Description { get; }

		/// <summary>
		/// Stream that produced the artifact
		/// </summary>
		public StreamId StreamId { get; }

		/// <summary>
		/// Change number
		/// </summary>
		public int Change { get; }

		/// <summary>
		/// Keys used to collate artifacts
		/// </summary>
		public IReadOnlyList<string> Keys { get; }

		/// <summary>
		/// List of metadata properties stored with the artifact, in the form 'Key=Value'
		/// </summary>
		public IReadOnlyList<string> Metadata { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public GetArtifactResponse(ArtifactId id, ArtifactName name, ArtifactType type, string? description, StreamId streamId, int change, IReadOnlyList<string> keys, IReadOnlyList<string> metadata)
		{
			Id = id;
			Name = name;
			Type = type;
			Description = description;
			StreamId = streamId;
			Change = change;
			Keys = keys;
			Metadata = metadata;
		}
	}

	/// <summary>
	/// Result of an artifact search
	/// </summary>
	public class FindArtifactsResponse
	{
		/// <summary>
		/// List of artifacts matching the search criteria
		/// </summary>
		public List<GetArtifactResponse> Artifacts { get; set; } = new List<GetArtifactResponse>();
	}

	/// <summary>
	/// Describes a file within an artifact
	/// </summary>
	public class GetArtifactFileEntryResponse
	{
		/// <summary>
		/// Name of this file
		/// </summary>
		public string Name { get; }

		/// <summary>
		/// Length of this entry
		/// </summary>
		public long Length { get; }

		/// <summary>
		/// Hash of the target node
		/// </summary>
		public IoHash Hash { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public GetArtifactFileEntryResponse(string name, long length, IoHash hash)
		{
			Name = name;
			Length = length;
			Hash = hash;
		}
	}

	/// <summary>
	/// Describes a file within an artifact
	/// </summary>
	public class GetArtifactDirectoryEntryResponse : GetArtifactDirectoryResponse
	{
		/// <summary>
		/// Name of this file
		/// </summary>
		public string Name { get; }

		/// <summary>
		/// Length of this entry
		/// </summary>
		public long Length { get; }

		/// <summary>
		/// Hash of the target node
		/// </summary>
		public IoHash Hash { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public GetArtifactDirectoryEntryResponse(string name, long length, IoHash hash)
		{
			Name = name;
			Length = length;
			Hash = hash;
		}
	}

	/// <summary>
	/// Describes a directory within an artifact
	/// </summary>
	public class GetArtifactDirectoryResponse
	{
		/// <summary>
		/// Names of sub-directories
		/// </summary>
		public List<GetArtifactDirectoryEntryResponse>? Directories { get; set; }

		/// <summary>
		/// Files within the directory
		/// </summary>
		public List<GetArtifactFileEntryResponse>? Files { get; set; }
	}

	/// <summary>
	/// Request to create a zip file with artifact data
	/// </summary>
	public class CreateZipRequest
	{
		/// <summary>
		/// Filter lines for the zip. Uses standard <see cref="FileFilter"/> syntax.
		/// </summary>
		public List<string> Filter { get; set; } = new List<string>();
	}
}
