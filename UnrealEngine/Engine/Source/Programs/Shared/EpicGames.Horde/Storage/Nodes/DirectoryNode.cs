// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Horde.Storage.Git;
using EpicGames.Serialization;
using Microsoft.Extensions.Logging;
using System;
using System.Buffers;
using System.Collections.Generic;
using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Threading;
using System.Threading.Tasks;

namespace EpicGames.Horde.Storage.Nodes
{
	/// <summary>
	/// Flags for a file entry
	/// </summary>
	[Flags]
	public enum FileEntryFlags
	{
		/// <summary>
		/// No other flags set
		/// </summary>
		None = 0,

		/// <summary>
		/// Indicates that the referenced file is executable
		/// </summary>
		Executable = 4,

		/// <summary>
		/// File should be stored as read-only
		/// </summary>
		ReadOnly = 8,

		/// <summary>
		/// File contents are utf-8 encoded text. Client may want to replace line-endings with OS-specific format.
		/// </summary>
		Text = 16,

		/// <summary>
		/// The data for this entry is a Perforce depot path and revision rather than the actual file contents.
		/// </summary>
		PerforceDepotPathAndRevision = 32,
	}

	/// <summary>
	/// Entry for a file within a directory node
	/// </summary>
	public class FileEntry : TreeNodeRef<FileNode>
	{
		/// <summary>
		/// Name of this file
		/// </summary>
		public Utf8String Name { get; }

		/// <summary>
		/// Flags for this file
		/// </summary>
		public FileEntryFlags Flags { get; set; }

		/// <summary>
		/// Length of this entry
		/// </summary>
		public long Length => (Node == null) ? _cachedLength : Node.Length;

		/// <summary>
		/// SHA1 hash of this file, with Git prefix
		/// </summary>
		public Sha1Hash GitHash { get; set; }

		/// <summary>
		/// Cached length of this node
		/// </summary>
		long _cachedLength;

		/// <summary>
		/// Constructor
		/// </summary>
		public FileEntry(DirectoryNode owner, Utf8String name, FileEntryFlags flags, FileNode node)
			: base(owner, node)
		{
			Name = name;
			Flags = flags;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public FileEntry(DirectoryNode owner, Utf8String name, FileEntryFlags flags, long length, ITreeBlobRef target)
			: base(owner, target)
		{
			Name = name;
			Flags = flags;
			
			_cachedLength = length;
		}

		/// <summary>
		/// Appends data to this file
		/// </summary>
		/// <param name="data">Data to append to the file</param>
		/// <param name="options">Options for chunking the data</param>
		/// <param name="writer">Writer for new node data</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public async Task AppendAsync(ReadOnlyMemory<byte> data, ChunkingOptions options, ITreeWriter writer, CancellationToken cancellationToken)
		{
			FileNode node = await ExpandAsync(cancellationToken);
			Node = await node.AppendAsync(data, options, writer, cancellationToken);
			MarkAsDirty();
		}

		/// <summary>
		/// Appends data to this file
		/// </summary>
		/// <param name="stream">Data to append to the file</param>
		/// <param name="options">Options for chunking the data</param>
		/// <param name="writer">Writer for new node data</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public async Task AppendAsync(Stream stream, ChunkingOptions options, ITreeWriter writer, CancellationToken cancellationToken)
		{
			byte[] buffer = new byte[4 * 1024];
			for (; ; )
			{
				int numBytes = await stream.ReadAsync(buffer, cancellationToken);
				if (numBytes == 0)
				{
					break;
				}
				await AppendAsync(buffer.AsMemory(0, numBytes), options, writer, cancellationToken);
			}
		}

		/// <inheritdoc/>
		protected override void OnCollapse()
		{
			_cachedLength = Node!.Length;
		}

		/// <inheritdoc/>
		public override string ToString() => IsDirty() ? $"{Name} (*)" : Name.ToString();
	}

	/// <summary>
	/// Entry for a directory within a directory node
	/// </summary>
	public class DirectoryEntry : TreeNodeRef<DirectoryNode>
	{
		/// <summary>
		/// Name of this directory
		/// </summary>
		public Utf8String Name { get; }

		/// <summary>
		/// Length of this directory tree
		/// </summary>
		public long Length => (Node == null) ? _cachedLength : Node.Length;

		/// <summary>
		/// Optional Git hash for the referenced directory object.
		/// </summary>
		public Sha1Hash GitHash { get; set; }

		/// <summary>
		/// Cached value for the length of this tree
		/// </summary>
		long _cachedLength;

		/// <summary>
		/// Constructor
		/// </summary>
		public DirectoryEntry(DirectoryNode owner, Utf8String name, DirectoryNode node)
			: base(owner, node)
		{
			Name = name;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public DirectoryEntry(DirectoryNode owner, Utf8String name, long length, ITreeBlobRef target)
			: base(owner, target)
		{
			Name = name;

			_cachedLength = length;
		}

		/// <inheritdoc/>
		protected override void OnCollapse()
		{
			_cachedLength = Node!.Length;
			if (_owner is DirectoryNode directoryNode && (directoryNode.Flags & DirectoryFlags.WithGitHashes) != 0)
			{
				GitHash = Node!.AsGitTree().GetHash();
			}
		}

		/// <inheritdoc/>
		public override string ToString() => IsDirty() ? $"{Name} (*)" : Name.ToString();
	}

	/// <summary>
	/// Flags for a directory node
	/// </summary>
	public enum DirectoryFlags
	{
		/// <summary>
		/// No flags specified
		/// </summary>
		None = 0,

		/// <summary>
		/// Includes hashes for all entries
		/// </summary>
		WithGitHashes = 1,
	}

	/// <summary>
	/// A directory node
	/// </summary>
	[TreeSerializer(typeof(DirectoryNodeSerializer))]
	public class DirectoryNode : TreeNode
	{
		internal const byte TypeId = (byte)'d';

		readonly Dictionary<Utf8String, FileEntry> _nameToFileEntry = new Dictionary<Utf8String, FileEntry>();
		readonly Dictionary<Utf8String, DirectoryEntry> _nameToDirectoryEntry = new Dictionary<Utf8String, DirectoryEntry>();

		/// <summary>
		/// Total size of this directory
		/// </summary>
		public long Length => Files.Sum(x => x.Length) + Directories.Sum(x => x.Length);

		/// <summary>
		/// Flags for this directory 
		/// </summary>
		public DirectoryFlags Flags { get; }

		/// <summary>
		/// All the files within this directory
		/// </summary>
		public IReadOnlyCollection<FileEntry> Files => _nameToFileEntry.Values;

		/// <summary>
		/// All the subdirectories within this directory
		/// </summary>
		public IReadOnlyCollection<DirectoryEntry> Directories => _nameToDirectoryEntry.Values;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="flags"></param>
		public DirectoryNode(DirectoryFlags flags)
		{
			Flags = flags;
		}

		/// <summary>
		/// Clear the contents of this directory
		/// </summary>
		public void Clear()
		{
			_nameToFileEntry.Clear();
			_nameToDirectoryEntry.Clear();
			MarkAsDirty();
		}

		/// <summary>
		/// Check whether an entry with the given name exists in this directory
		/// </summary>
		/// <param name="name">Name of the entry to search for</param>
		/// <returns>True if the entry exists</returns>
		public bool Contains(Utf8String name) => TryGetFileEntry(name, out _) || TryGetDirectoryEntry(name, out _);

		/// <inheritdoc/>
		public override IReadOnlyList<TreeNodeRef> GetReferences()
		{
			List<TreeNodeRef> refs = new List<TreeNodeRef>();
			refs.AddRange(Files);
			refs.AddRange(Directories);
			return refs;
		}

		#region File operations

		/// <summary>
		/// Adds a new directory with the given name
		/// </summary>
		/// <param name="name">Name of the new directory</param>
		/// <param name="flags">Flags for the new file</param>
		/// <returns>The new directory object</returns>
		public FileEntry AddFile(Utf8String name, FileEntryFlags flags)
		{
			if (TryGetDirectoryEntry(name, out _))
			{
				throw new ArgumentException($"A directory with the name {name} already exists");
			}

			FileNode newNode = new LeafFileNode();

			FileEntry entry = new FileEntry(this, name, flags, newNode);
			_nameToFileEntry[name] = entry;
			MarkAsDirty();

			return entry;
		}

		/// <summary>
		/// Adds a new directory with the given name
		/// </summary>
		/// <param name="name">Name of the new directory</param>
		/// <param name="flags">Flags for the new file</param>
		/// <param name="stream">The stream to read from</param>
		/// <param name="options">Options for chunking the data</param>
		/// <param name="writer">Writer for new node data</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The new directory object</returns>
		public async Task<FileEntry> AddFileAsync(Utf8String name, FileEntryFlags flags, Stream stream, ChunkingOptions options, ITreeWriter writer, CancellationToken cancellationToken = default)
		{
			FileEntry entry = AddFile(name, flags);
			await entry.AppendAsync(stream, options, writer, cancellationToken);
			return entry;
		}

		/// <summary>
		/// Finds or adds a file with the given path
		/// </summary>
		/// <param name="path">Path to the file</param>
		/// <param name="flags">Flags for the new file</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The new directory object</returns>
		public async ValueTask<FileEntry> AddFileByPathAsync(Utf8String path, FileEntryFlags flags, CancellationToken cancellationToken = default)
		{
			DirectoryNode directory = this;

			Utf8String remainingPath = path;
			if (remainingPath[0] == '/' || remainingPath[0] == '\\')
			{
				remainingPath = remainingPath.Substring(1);
			}

			for (; ; )
			{
				int length = 0;
				for (; ; length++)
				{
					if (length == remainingPath.Length)
					{
						return directory.AddFile(remainingPath, flags);
					}

					byte character = remainingPath[length];
					if (character == '\\' || character == '/')
					{
						break;
					}
				}

				if (length > 0)
				{
					directory = await directory.FindOrAddDirectoryAsync(remainingPath.Slice(0, length), cancellationToken);
				}
				remainingPath = remainingPath.Slice(length + 1);
			}
		}

		/// <summary>
		/// Attempts to get a file entry with the given name
		/// </summary>
		/// <param name="name">Name of the file</param>
		/// <returns>Entry for the given name</returns>
		public FileEntry GetFileEntry(Utf8String name) => _nameToFileEntry[name];

		/// <summary>
		/// Attempts to get a file entry with the given name
		/// </summary>
		/// <param name="name">Name of the file</param>
		/// <param name="entry">Entry for the file</param>
		/// <returns>True if the file was found</returns>
		public bool TryGetFileEntry(Utf8String name, [NotNullWhen(true)] out FileEntry? entry) => _nameToFileEntry.TryGetValue(name, out entry);

		/// <summary>
		/// Deletes the file entry with the given name
		/// </summary>
		/// <param name="name">Name of the entry to delete</param>
		/// <returns>True if the entry was found, false otherwise</returns>
		public bool DeleteFile(Utf8String name)
		{
			if (_nameToFileEntry.Remove(name))
			{
				MarkAsDirty();
				return true;
			}
			return false;
		}

		/// <summary>
		/// Deletes a file with the given path
		/// </summary>
		/// <param name="path"></param>
		/// <param name="cancellationToken"></param>
		/// <returns></returns>
		public async ValueTask<bool> DeleteFileByPathAsync(Utf8String path, CancellationToken cancellationToken)
		{
			Utf8String remainingPath = path;
			for (DirectoryNode? directory = this; directory != null;)
			{
				int length = remainingPath.IndexOf('/');
				if (length == -1)
				{
					return directory.DeleteFile(remainingPath);
				}
				if (length > 0)
				{
					directory = await directory.FindDirectoryAsync(remainingPath.Slice(0, length), cancellationToken);
				}
				remainingPath = remainingPath.Slice(length + 1);
			}
			return false;
		}

		#endregion

		#region Directory operations

		/// <summary>
		/// Adds a new directory with the given name
		/// </summary>
		/// <param name="name">Name of the new directory</param>
		/// <returns>The new directory object</returns>
		public DirectoryNode AddDirectory(Utf8String name)
		{
			if (TryGetFileEntry(name, out _))
			{
				throw new ArgumentException($"A file with the name '{name}' already exists in this directory", nameof(name));
			}

			DirectoryNode node = new DirectoryNode(Flags);

			DirectoryEntry entry = new DirectoryEntry(this, name, node);
			_nameToDirectoryEntry.Add(name, entry);
			MarkAsDirty();

			return node;
		}

		/// <summary>
		/// Get a directory entry with the given name
		/// </summary>
		/// <param name="name">Name of the directory</param>
		/// <returns>The entry with the given name</returns>
		public DirectoryEntry GetDirectoryEntry(Utf8String name) => _nameToDirectoryEntry[name];

		/// <summary>
		/// Attempts to get a directory entry with the given name
		/// </summary>
		/// <param name="name">Name of the directory</param>
		/// <param name="entry">Entry for the directory</param>
		/// <returns>True if the directory was found</returns>
		public bool TryGetDirectoryEntry(Utf8String name, [NotNullWhen(true)] out DirectoryEntry? entry) => _nameToDirectoryEntry.TryGetValue(name, out entry);

		/// <summary>
		/// Tries to get a directory with the given name
		/// </summary>
		/// <param name="name">Name of the new directory</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The new directory object</returns>
		public async ValueTask<DirectoryNode?> FindDirectoryAsync(Utf8String name, CancellationToken cancellationToken)
		{
			if (TryGetDirectoryEntry(name, out DirectoryEntry? entry))
			{
				return await entry.ExpandAsync(cancellationToken);
			}
			else
			{
				return null;
			}
		}

		/// <summary>
		/// Tries to get a directory with the given name
		/// </summary>
		/// <param name="name">Name of the new directory</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The new directory object</returns>
		public async ValueTask<DirectoryNode> FindOrAddDirectoryAsync(Utf8String name, CancellationToken cancellationToken)
		{
			DirectoryNode? directory = await FindDirectoryAsync(name, cancellationToken);
			if (directory == null)
			{
				directory = AddDirectory(name);
			}
			return directory;
		}

		/// <summary>
		/// Deletes the file entry with the given name
		/// </summary>
		/// <param name="name">Name of the entry to delete</param>
		/// <returns>True if the entry was found, false otherwise</returns>
		public bool DeleteDirectory(Utf8String name)
		{
			if (_nameToDirectoryEntry.Remove(name))
			{
				MarkAsDirty();
				return true;
			}
			return false;
		}

		#endregion

		/// <inheritdoc/>
		public override async Task<ITreeBlob> SerializeAsync(ITreeWriter writer, CancellationToken cancellationToken)
		{
			List<ITreeBlobRef> refs = new List<ITreeBlobRef>();

			List<FileEntry> fileEntries = _nameToFileEntry.Values.OrderBy(x => x.Name).ToList();
			foreach (FileEntry fileEntry in fileEntries)
			{
				refs.Add(await fileEntry.CollapseAsync(writer, cancellationToken));
			}

			List<DirectoryEntry> directoryEntries = _nameToDirectoryEntry.Values.OrderBy(x => x.Name).ToList();
			foreach (DirectoryEntry directoryEntry in directoryEntries)
			{
				refs.Add(await directoryEntry.CollapseAsync(writer, cancellationToken));
			}

			ReadOnlySequence<byte> data = SerializeData(Flags, fileEntries, directoryEntries);
			return new NewTreeBlob(data, refs);
		}

		static ReadOnlySequence<byte> SerializeData(DirectoryFlags flags, List<FileEntry> fileEntries, List<DirectoryEntry> directoryEntries)
		{
			// Measure the required size of the write buffer
			int size = 1;
			size += VarInt.MeasureUnsigned((ulong)flags);
			size += VarInt.MeasureUnsigned(fileEntries.Count);
			foreach (FileEntry fileEntry in fileEntries)
			{
				size += (fileEntry.Name.Length + 1) + 1 + VarInt.MeasureUnsigned((ulong)fileEntry.Length);
			}
			size += VarInt.MeasureUnsigned(directoryEntries.Count);
			foreach (DirectoryEntry directoryEntry in directoryEntries)
			{
				size += (directoryEntry.Name.Length + 1) + VarInt.MeasureUnsigned((ulong)directoryEntry.Length);
			}
			if ((flags & DirectoryFlags.WithGitHashes) != 0)
			{
				size += (directoryEntries.Count + fileEntries.Count) * IoHash.NumBytes;
			}

			// Allocate the buffer and copy the node to it
			byte[] data = new byte[size];
			Span<byte> span = data.AsSpan();

			span[0] = TypeId;
			span = span.Slice(1);

			int directoryFlagBytes = VarInt.WriteUnsigned(span, (ulong)flags);
			span = span.Slice(directoryFlagBytes);

			int fileCountBytes = VarInt.WriteUnsigned(span, fileEntries.Count);
			span = span.Slice(fileCountBytes);

			foreach (FileEntry fileEntry in fileEntries)
			{
				fileEntry.Name.Span.CopyTo(span);
				span = span.Slice(fileEntry.Name.Length + 1);

				span[0] = (byte)fileEntry.Flags;
				span = span.Slice(1);

				int lengthBytes = VarInt.WriteUnsigned(span, fileEntry.Length);
				span = span.Slice(lengthBytes);

				if ((flags & DirectoryFlags.WithGitHashes) != 0)
				{
					fileEntry.GitHash.CopyTo(span);
					span = span.Slice(Sha1Hash.NumBytes);
				}
			}

			int directoryCountBytes = VarInt.WriteUnsigned(span, directoryEntries.Count);
			span = span.Slice(directoryCountBytes);

			foreach (DirectoryEntry directoryEntry in directoryEntries)
			{
				directoryEntry.Name.Span.CopyTo(span);
				span = span.Slice(directoryEntry.Name.Length + 1);

				int lengthBytes = VarInt.WriteUnsigned(span, directoryEntry.Length);
				span = span.Slice(lengthBytes);

				if ((flags & DirectoryFlags.WithGitHashes) != 0)
				{
					directoryEntry.GitHash.CopyTo(span);
					span = span.Slice(Sha1Hash.NumBytes);
				}
			}

			Debug.Assert(span.Length == 0);
			return new ReadOnlySequence<byte>(data);
		}

		/// <summary>
		/// Deserialize a directory node from data
		/// </summary>
		/// <param name="source">The source node to deserialize</param>
		/// <returns>The deserialized directory node</returns>
		public static DirectoryNode Deserialize(ITreeBlob source)
		{
			ReadOnlySpan<byte> span = source.Data.AsSingleSegment().Span;
			if (span[0] != TypeId)
			{
				throw new InvalidOperationException("Invalid signature byte for directory");
			}

			span = span.Slice(1);

			DirectoryFlags directoryFlags = (DirectoryFlags)VarInt.ReadUnsigned(span, out int directoryFlagBytes);
			span = span.Slice(directoryFlagBytes);

			DirectoryNode node = new DirectoryNode(directoryFlags);

			int fileCount = (int)VarInt.ReadUnsigned(span, out int fileCountBytes);
			span = span.Slice(fileCountBytes);

			int childIdx = 0;

			node._nameToFileEntry.EnsureCapacity(fileCount);
			for (int idx = 0; idx < fileCount; idx++)
			{
				int nameLen = span.IndexOf((byte)0);
				Utf8String name = new Utf8String(span.Slice(0, nameLen).ToArray());
				span = span.Slice(nameLen + 1);

				FileEntryFlags flags = (FileEntryFlags)VarInt.ReadUnsigned(span, out int flagBytes);
				span = span.Slice(flagBytes);

				long length = (long)VarInt.ReadUnsigned(span, out int lengthBytes);
				span = span.Slice(lengthBytes);

				ITreeBlobRef child = source.Refs[childIdx++];
				FileEntry entry = new FileEntry(node, name, flags, length, child);

				if ((directoryFlags & DirectoryFlags.WithGitHashes) != 0)
				{
					entry.GitHash = new Sha1Hash(span);
					span = span.Slice(Sha1Hash.NumBytes);
				}

				node._nameToFileEntry[name] = entry;
			}

			int directoryCount = (int)VarInt.ReadUnsigned(span, out int directoryCountBytes);
			span = span.Slice(directoryCountBytes);

			node._nameToDirectoryEntry.EnsureCapacity(directoryCount);
			for (int idx = 0; idx < directoryCount; idx++)
			{
				int nameLen = span.IndexOf((byte)0);
				Utf8String name = new Utf8String(span.Slice(0, nameLen).ToArray());
				span = span.Slice(nameLen + 1);

				long length = (long)VarInt.ReadUnsigned(span, out int lengthBytes);
				span = span.Slice(lengthBytes);

				ITreeBlobRef child = source.Refs[childIdx++];
				DirectoryEntry entry = new DirectoryEntry(node, name, length, child);

				if ((directoryFlags & DirectoryFlags.WithGitHashes) != 0)
				{
					entry.GitHash = new Sha1Hash(span);
					span = span.Slice(Sha1Hash.NumBytes);
				}

				node._nameToDirectoryEntry[name] = entry;
			}

			Debug.Assert(span.Length == 0);
			return node;
		}

		/// <summary>
		/// Adds files from a directory on disk
		/// </summary>
		/// <param name="directoryInfo"></param>
		/// <param name="options">Options for chunking file content</param>
		/// <param name="writer">Writer for new node data</param>
		/// <param name="logger"></param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		public async Task CopyFromDirectoryAsync(DirectoryInfo directoryInfo, ChunkingOptions options, ITreeWriter writer, ILogger logger, CancellationToken cancellationToken)
		{
			foreach (DirectoryInfo subDirectoryInfo in directoryInfo.EnumerateDirectories())
			{
				logger.LogInformation("Adding {Directory}", subDirectoryInfo.FullName);
				DirectoryNode subDirectoryNode = AddDirectory(subDirectoryInfo.Name);
				await subDirectoryNode.CopyFromDirectoryAsync(subDirectoryInfo, options, writer, logger, cancellationToken);
			}
			foreach (FileInfo fileInfo in directoryInfo.EnumerateFiles())
			{
				logger.LogInformation("Adding {File}", fileInfo.FullName);
				using Stream stream = fileInfo.OpenRead();
				await AddFileAsync(fileInfo.Name, 0, stream, options, writer, cancellationToken);
			}
		}

		/// <summary>
		/// Utility function to allow extracting a packed directory to disk
		/// </summary>
		/// <param name="directoryInfo"></param>
		/// <param name="logger"></param>
		/// <param name="cancellationToken"></param>
		/// <returns></returns>
		public async Task CopyToDirectoryAsync(DirectoryInfo directoryInfo, ILogger logger, CancellationToken cancellationToken)
		{
			directoryInfo.Create();

			List<Task> tasks = new List<Task>();
			foreach (FileEntry fileEntry in Files)
			{
				FileInfo fileInfo = new FileInfo(Path.Combine(directoryInfo.FullName, fileEntry.Name.ToString()));
				FileNode fileNode = await fileEntry.ExpandAsync(cancellationToken);
				logger.LogInformation("Writing {File}", fileInfo.FullName);
				tasks.Add(Task.Run(() => fileNode.CopyToFileAsync(fileInfo, cancellationToken), cancellationToken));
			}
			foreach (DirectoryEntry directoryEntry in Directories)
			{
				DirectoryInfo subDirectoryInfo = directoryInfo.CreateSubdirectory(directoryEntry.Name.ToString());
				DirectoryNode subDirectoryNode = await directoryEntry.ExpandAsync(cancellationToken);
				logger.LogInformation("Writing {Dir}", subDirectoryInfo.FullName);
				tasks.Add(Task.Run(() => subDirectoryNode.CopyToDirectoryAsync(subDirectoryInfo, logger, cancellationToken), cancellationToken));
			}

			await Task.WhenAll(tasks);
		}

		/// <summary>
		/// Creates a Git tree object for this directory
		/// </summary>
		/// <returns>Tree object</returns>
		public GitTree AsGitTree()
		{
			GitTree tree = new GitTree();
			foreach (FileEntry fileEntry in _nameToFileEntry.Values)
			{
				tree.Entries.Add(new GitTreeEntry(GitFileMode.File, fileEntry.Name, fileEntry.GitHash));
			}
			foreach (DirectoryEntry directoryEntry in _nameToDirectoryEntry.Values)
			{
				tree.Entries.Add(new GitTreeEntry(GitFileMode.Tree, directoryEntry.Name, directoryEntry.GitHash));
			}
			tree.Entries.SortBy(x => x.Name);
			return tree;
		}
	}

	/// <summary>
	/// Factory for creating and serializing <see cref="DirectoryNode"/> objects
	/// </summary>
	public class DirectoryNodeSerializer : TreeNodeSerializer<DirectoryNode>
	{
		/// <inheritdoc/>
		public override DirectoryNode Deserialize(ITreeBlob blob) => DirectoryNode.Deserialize(blob);
	}

	/// <summary>
	/// Extension methods for <see cref="DirectoryNode"/>
	/// </summary>
	public static class DirectoryNodeExtensions
	{
	}
}
