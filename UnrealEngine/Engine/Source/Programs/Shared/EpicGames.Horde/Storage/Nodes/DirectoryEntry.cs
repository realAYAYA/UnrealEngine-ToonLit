// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;

namespace EpicGames.Horde.Storage.Nodes
{
	/// <summary>
	/// Entry for a directory within a directory node
	/// </summary>
	public class DirectoryEntry : DirectoryNodeRef
	{
		/// <summary>
		/// Name of this directory
		/// </summary>
		public Utf8String Name { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public DirectoryEntry(Utf8String name, long length, NodeRef<DirectoryNode> nodeRef)
			: base(length, nodeRef)
		{
			Name = name;
		}

		/// <summary>
		/// Deserializing constructor
		/// </summary>
		/// <param name="reader"></param>
		public DirectoryEntry(NodeReader reader)
			: base(reader)
		{
			Name = reader.ReadUtf8String();
		}

		/// <summary>
		/// Serialize this directory entry to disk
		/// </summary>
		/// <param name="writer"></param>
		public override void Serialize(NodeWriter writer)
		{
			base.Serialize(writer);

			writer.WriteUtf8String(Name);
		}

		/// <inheritdoc/>
		public override string ToString() => Name.ToString();
	}
}
