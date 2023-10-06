// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;

namespace EpicGames.Horde.Storage.Nodes
{
	/// <summary>
	/// Reference to a directory node, including the target hash and length
	/// </summary>
	public class DirectoryNodeRef : NodeRef<DirectoryNode>
	{
		/// <summary>
		/// Length of this directory tree
		/// </summary>
		public long Length { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public DirectoryNodeRef(long length, NodeRef<DirectoryNode> nodeRef)
			: base(nodeRef.Handle)
		{
			Length = length;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public DirectoryNodeRef(NodeReader reader)
			: base(reader)
		{
			Length = (long)reader.ReadUnsignedVarInt();
		}

		/// <summary>
		/// Serialize this directory entry to disk
		/// </summary>
		/// <param name="writer"></param>
		public override void Serialize(NodeWriter writer)
		{
			base.Serialize(writer);

			writer.WriteUnsignedVarInt((ulong)Length);
		}
	}
}
