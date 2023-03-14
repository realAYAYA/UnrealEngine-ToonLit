// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using Horde.Build.Streams;
using Horde.Build.Users;
using Horde.Build.Utilities;

namespace Horde.Build.Perforce
{
	using CommitId = ObjectId<ICommit>;
	using StreamId = StringId<IStream>;
	using UserId = ObjectId<IUser>;

	/// <summary>
	/// Stores metadata about a commit
	/// </summary>
	public interface ICommit
	{
		/// <summary>
		/// Unique id for this document
		/// </summary>
		public CommitId Id { get; }

		/// <summary>
		/// The stream id
		/// </summary>
		public StreamId StreamId { get; }

		/// <summary>
		/// The changelist number
		/// </summary>
		public int Change { get; }

		/// <summary>
		/// The change that this commit originates from
		/// </summary>
		public int OriginalChange { get; }

		/// <summary>
		/// The author user id
		/// </summary>
		public UserId AuthorId { get; }

		/// <summary>
		/// The owner of this change, if different from the author (due to Robomerge)
		/// </summary>
		public UserId OwnerId { get; }

		/// <summary>
		/// Changelist description
		/// </summary>
		public string Description { get; }

		/// <summary>
		/// Base path for all files in the change
		/// </summary>
		public string BasePath { get; }

		/// <summary>
		/// Date/time that change was committed
		/// </summary>
		public DateTime DateUtc { get; }
	}

	/// <summary>
	/// New commit object
	/// </summary>
	public class NewCommit
	{
		/// <inheritdoc cref="ICommit.StreamId"/>
		public StreamId StreamId { get; set; }

		/// <inheritdoc cref="ICommit.Change"/>
		public int Change { get; set; }

		/// <inheritdoc cref="ICommit.OriginalChange"/>
		public int OriginalChange { get; set; }

		/// <inheritdoc cref="ICommit.AuthorId"/>
		public UserId AuthorId { get; set; }

		/// <inheritdoc cref="ICommit.OwnerId"/>
		public UserId? OwnerId { get; set; }

		/// <inheritdoc cref="ICommit.Description"/>
		public string Description { get; set; }

		/// <inheritdoc cref="ICommit.BasePath"/>
		public string BasePath { get; set; }

		/// <inheritdoc cref="ICommit.DateUtc"/>
		public DateTime DateUtc { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		public NewCommit(ICommit commit)
			: this(commit.StreamId, commit.Change, commit.OriginalChange, commit.AuthorId, commit.OwnerId, commit.Description, commit.BasePath, commit.DateUtc)
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public NewCommit(StreamId streamId, int change, int originalChange, UserId authorId, UserId ownerId, string description, string basePath, DateTime dateUtc)
		{
			StreamId = streamId;
			Change = change;
			OriginalChange = originalChange;
			AuthorId = authorId;
			OwnerId = ownerId;
			Description = description;
			BasePath = basePath;
			DateUtc = dateUtc;
		}
	}
}
