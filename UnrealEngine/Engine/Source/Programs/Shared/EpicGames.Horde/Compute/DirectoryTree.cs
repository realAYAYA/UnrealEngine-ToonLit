// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Serialization;
using System.Collections.Generic;

namespace EpicGames.Horde.Compute
{
	/// <summary>
	/// Stores information about a directory in an action's workspace
	/// </summary>
	public class DirectoryTree
	{
		/// <summary>
		/// List of files within the directory, sorted by name
		/// </summary>
		[CbField("f")]
		public List<FileNode> Files { get; } = new List<FileNode>();

		/// <summary>
		/// Sub-directories within this directory, sorted by name
		/// </summary>
		[CbField("d")]
		public List<DirectoryNode> Directories { get; } = new List<DirectoryNode>();
	}

	/// <summary>
	/// Reference to a named file within a <see cref="DirectoryTree"/>
	/// </summary>
	public class FileNode
	{
		/// <summary>
		/// Name of the directory
		/// </summary>
		[CbField("n")]
		public Utf8String Name { get; set; }

		/// <summary>
		/// Hash of bulk data for the file stored in CAS.
		/// </summary>
		[CbField("h")]
		public CbBinaryAttachment Hash { get; set; }

		/// <summary>
		/// Size of the file
		/// </summary>
		[CbField("s")]
		public long Size { get; set; }

		/// <summary>
		/// The file attributes
		/// </summary>
		[CbField("a")]
		public int Attributes { get; set; }
		
		/// <summary>
		/// Whether the file is compressed or not.
		/// </summary>
		[CbField("c")]
		public bool IsCompressed { get; set; }

		/// <summary>
		/// Private constructor for serialization
		/// </summary>
		private FileNode()
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public FileNode(Utf8String name, CbBinaryAttachment hash, long size, int attributes, bool isCompressed)
		{
			Name = name;
			Hash = hash;
			Size = size;
			Attributes = attributes;
			IsCompressed = isCompressed;
		}
	}

	/// <summary>
	/// Reference to a named directory within a <see cref="DirectoryTree"/>
	/// </summary>
	public class DirectoryNode
	{
		/// <summary>
		/// Name of the directory
		/// </summary>
		[CbField("n")]
		public Utf8String Name { get; set; }

		/// <summary>
		/// Hash of a <see cref="CbObject"/> encoded <see cref="DirectoryTree"/> stored in CAS.
		/// </summary>
		[CbField("h")]
		public CbObjectAttachment Hash { get; set; }

		/// <summary>
		/// Private constructor for serialization
		/// </summary>
		private DirectoryNode()
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public DirectoryNode(Utf8String name, CbObjectAttachment hash)
		{
			Name = name;
			Hash = hash;
		}
	}
}
