// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using System;
using System.Collections.Generic;

namespace EpicGames.Horde.Storage.Nodes
{
	/// <summary>
	/// A node representing commit metadata
	/// </summary>
	[NodeType("{64D50724-6B22-41C0-A890-B51CD6241817}", 1)]
	public class CommitNode : Node
	{
		/// <summary>
		/// The commit number
		/// </summary>
		public int Number { get; set; }

		/// <summary>
		/// Reference to the parent commit
		/// </summary>
		public NodeRef<CommitNode>? Parent { get; set; }

		/// <summary>
		/// Human readable name of the author of this change
		/// </summary>
		public string Author { get; set; }

		/// <summary>
		/// Optional unique identifier for the author. May be an email address, user id, etc...
		/// </summary>
		public string? AuthorId { get; set; }

		/// <summary>
		/// Human readable name of the committer of this change
		/// </summary>
		public string? Committer { get; set; }

		/// <summary>
		/// Optional unique identifier for the committer. May be an email address, user id, etc...
		/// </summary>
		public string? CommitterId { get; set; }

		/// <summary>
		/// Message for this commit
		/// </summary>
		public string Message { get; set; }

		/// <summary>
		/// Time that this commit was created
		/// </summary>
		public DateTime Time { get; set; }

		/// <summary>
		/// Contents of the tree at this commit
		/// </summary>
		public DirectoryNodeRef Contents { get; set; }

		/// <summary>
		/// Metadata for this commit, keyed by arbitrary GUID
		/// </summary>
		public Dictionary<Guid, NodeRef> Metadata { get; } = new Dictionary<Guid, NodeRef>();

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="number">Commit number</param>
		/// <param name="parent">The parent commit</param>
		/// <param name="author">Author of this commit</param>
		/// <param name="message">Message for the commit</param>
		/// <param name="time">The commit time</param>
		/// <param name="contents">Contents of the tree at this commit</param>
		public CommitNode(int number, NodeRef<CommitNode>? parent, string author, string message, DateTime time, DirectoryNodeRef contents)
		{
			Number = number;
			Parent = parent;
			Author = author;
			Message = message;
			Time = time;
			Contents = contents;
		}

		/// <summary>
		/// Deserializing constructor
		/// </summary>
		/// <param name="reader"></param>
		public CommitNode(NodeReader reader)
		{
			Number = (int)reader.ReadUnsignedVarInt();
			Parent = reader.ReadOptionalNodeRef<CommitNode>();
			Author = reader.ReadString();
			AuthorId = reader.ReadOptionalString();
			Committer = reader.ReadOptionalString();
			CommitterId = reader.ReadOptionalString();
			Message = reader.ReadString();
			Time = reader.ReadDateTime();
			Contents = new DirectoryNodeRef(reader);
			Metadata = reader.ReadDictionary(() => reader.ReadGuid(), () => reader.ReadNodeRef());
		}

		/// <inheritdoc/>
		public override void Serialize(NodeWriter writer)
		{
			writer.WriteUnsignedVarInt(Number);
			writer.WriteOptionalNodeRef(Parent);
			writer.WriteString(Author);
			writer.WriteOptionalString(AuthorId);
			writer.WriteOptionalString(Committer);
			writer.WriteOptionalString(CommitterId);
			writer.WriteString(Message);
			writer.WriteDateTime(Time);
			writer.WriteNodeRef(Contents);
			writer.WriteDictionary(Metadata, key => writer.WriteGuid(key), value => writer.WriteNodeRef(value));
		}
	}
}
