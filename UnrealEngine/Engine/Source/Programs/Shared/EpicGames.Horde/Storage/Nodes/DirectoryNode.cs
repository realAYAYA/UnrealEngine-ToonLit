// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Horde.Logs;
using Microsoft.CodeAnalysis.Options;
using Microsoft.Extensions.Logging;
using System;
using System.Buffers;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.IO;
using System.IO.Compression;
using System.IO.Pipelines;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace EpicGames.Horde.Storage.Nodes
{
	/// <summary>
	/// Stats reported for copy operations
	/// </summary>
	public interface ICopyStats
	{
		/// <summary>
		/// Number of files that have been copied
		/// </summary>
		int CopiedCount { get; }

		/// <summary>
		/// Total size of data to be copied
		/// </summary>
		long CopiedSize { get; }

		/// <summary>
		/// Total number of files to copy
		/// </summary>
		int TotalCount { get; }

		/// <summary>
		/// Total size of data to copy
		/// </summary>
		long TotalSize { get; }
	}

	/// <summary>
	/// Progress logger for writing copy stats
	/// </summary>
	public class CopyStatsLogger : IProgress<ICopyStats>
	{
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public CopyStatsLogger(ILogger logger) => _logger = logger;

		/// <inheritdoc/>
		public void Report(ICopyStats stats)
		{
			_logger.LogInformation("Copied {NumFiles:n0}/{TotalFiles:n0} ({Size:n1}/{TotalSize:n1}mb, {Pct}%)", stats.CopiedCount, stats.TotalCount, stats.CopiedSize / (1024.0 * 1024.0), stats.TotalSize / (1024.0 * 1024.0), (int)((Math.Max(stats.CopiedCount, 1) * 100) / Math.Max(stats.TotalCount, 1)));
		}
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
	}

	/// <summary>
	/// A directory node
	/// </summary>
	[NodeType("{0714EC11-291A-4D07-867F-E78AD6809979}", 1)]
	public class DirectoryNode : Node
	{
		readonly Dictionary<Utf8String, FileEntry> _nameToFileEntry = new Dictionary<Utf8String, FileEntry>();
		readonly Dictionary<Utf8String, DirectoryEntry> _nameToDirectoryEntry = new Dictionary<Utf8String, DirectoryEntry>();

		/// <summary>
		/// Total size of this directory
		/// </summary>
		public long Length => _nameToFileEntry.Values.Sum(x => x.Length) + _nameToDirectoryEntry.Values.Sum(x => x.Length);

		/// <summary>
		/// Flags for this directory 
		/// </summary>
		public DirectoryFlags Flags { get; }

		/// <summary>
		/// All the files within this directory
		/// </summary>
		public IReadOnlyCollection<FileEntry> Files => _nameToFileEntry.Values;

		/// <summary>
		/// Map of name to file entry
		/// </summary>
		public IReadOnlyDictionary<Utf8String, FileEntry> NameToFile => _nameToFileEntry;

		/// <summary>
		/// All the subdirectories within this directory
		/// </summary>
		public IReadOnlyCollection<DirectoryEntry> Directories => _nameToDirectoryEntry.Values;

		/// <summary>
		/// Map of name to file entry
		/// </summary>
		public IReadOnlyDictionary<Utf8String, DirectoryEntry> NameToDirectory => _nameToDirectoryEntry;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="flags"></param>
		public DirectoryNode(DirectoryFlags flags = DirectoryFlags.None)
		{
			Flags = flags;
		}

		/// <summary>
		/// Deserialization constructor
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		public DirectoryNode(NodeReader reader)
		{
			Flags = (DirectoryFlags)reader.ReadUnsignedVarInt();

			int fileCount = (int)reader.ReadUnsignedVarInt();
			_nameToFileEntry.EnsureCapacity(fileCount);

			for (int idx = 0; idx < fileCount; idx++)
			{
				FileEntry entry = new FileEntry(reader);
				_nameToFileEntry[entry.Name] = entry;
			}

			int directoryCount = (int)reader.ReadUnsignedVarInt();
			_nameToDirectoryEntry.EnsureCapacity(directoryCount);

			for (int idx = 0; idx < directoryCount; idx++)
			{
				DirectoryEntry entry = new DirectoryEntry(reader);
				_nameToDirectoryEntry[entry.Name] = entry;
			}
		}

		/// <inheritdoc/>
		public override void Serialize(NodeWriter writer)
		{
			writer.WriteUnsignedVarInt((ulong)Flags);

			writer.WriteUnsignedVarInt(Files.Count);
			foreach (FileEntry fileEntry in _nameToFileEntry.Values)
			{
				writer.WriteNodeRef(fileEntry);
			}

			writer.WriteUnsignedVarInt(Directories.Count);
			foreach (DirectoryEntry directoryEntry in _nameToDirectoryEntry.Values)
			{
				writer.WriteNodeRef(directoryEntry);
			}
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

		#region File operations

		/// <summary>
		/// Adds a new file entry to this directory
		/// </summary>
		/// <param name="entry">The entry to add</param>
		public void AddFile(FileEntry entry)
		{
			_nameToFileEntry[entry.Name] = entry;
			MarkAsDirty();
		}

		/// <summary>
		/// Adds a new directory with the given name
		/// </summary>
		/// <param name="name">Name of the new directory</param>
		/// <param name="flags">Flags for the new file</param>
		/// <param name="length">Length of the file</param>
		/// <param name="dataRef">Handle to the file data</param>
		/// <returns>The new directory object</returns>
		public FileEntry AddFile(Utf8String name, FileEntryFlags flags, long length, NodeRef<ChunkedDataNode> dataRef)
		{
			FileEntry entry = new FileEntry(name, flags, length, dataRef);
			AddFile(entry);
			return entry;
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
		/// Attempts to get a file entry from a path
		/// </summary>
		/// <param name="path">Path to the directory</param>
		/// <param name="cancellationToken">Cancellation token</param>
		/// <returns>The directory with the given path, or null if it was not found</returns>
		public async ValueTask<FileEntry?> GetFileEntryByPathAsync(Utf8String path, CancellationToken cancellationToken)
		{
			FileEntry? fileEntry;

			int slashIdx = path.LastIndexOf('/');
			if (slashIdx == -1)
			{
				if (!TryGetFileEntry(path, out fileEntry))
				{
					return null;
				}
			}
			else
			{
				DirectoryNode? directoryNode = await GetDirectoryByPathAsync(path.Slice(0, slashIdx), cancellationToken);
				if (directoryNode == null)
				{
					return null;
				}
				if (!directoryNode.TryGetFileEntry(path.Slice(slashIdx + 1), out fileEntry))
				{
					return null;
				}
			}

			return fileEntry;
		}

		/// <summary>
		/// Attempts to get a directory entry from a path
		/// </summary>
		/// <param name="path">Path to the directory</param>
		/// <param name="cancellationToken">Cancellation token</param>
		/// <returns>The directory with the given path, or null if it was not found</returns>
		public ValueTask<DirectoryNode?> GetDirectoryByPathAsync(Utf8String path, CancellationToken cancellationToken) => GetDirectoryByPathAsync(this, path, cancellationToken);

		static async ValueTask<DirectoryNode?> GetDirectoryByPathAsync(DirectoryNode directoryNode, Utf8String path, CancellationToken cancellationToken)
		{
			while (path.Length > 0)
			{
				Utf8String directoryName;

				int slashIdx = path.IndexOf('/');
				if (slashIdx == -1)
				{
					directoryName = path;
					path = Utf8String.Empty;
				}
				else
				{
					directoryName = path.Slice(0, slashIdx);
					path = path.Slice(slashIdx + 1);
				}

				DirectoryEntry? directoryEntry;
				if (!directoryNode.TryGetDirectoryEntry(directoryName, out directoryEntry))
				{
					return null;
				}

				directoryNode = await directoryEntry.ExpandAsync(cancellationToken);
			}
			return directoryNode;
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
		/// <param name="entry">Name of the new directory</param>
		public void AddDirectory(DirectoryEntry entry)
		{
			if (TryGetFileEntry(entry.Name, out _))
			{
				throw new ArgumentException($"A file with the name '{entry.Name}' already exists in this directory", nameof(entry));
			}

			_nameToDirectoryEntry.Add(entry.Name, entry);
			MarkAsDirty();
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

		/// <summary>
		/// Reports progress info back to callers
		/// </summary>
		class CopyStats : ICopyStats
		{
			readonly object _lockObject = new object();
			readonly Stopwatch _timer = Stopwatch.StartNew();
			readonly IProgress<ICopyStats> _progress;

			public int CopiedCount { get; set; }
			public long CopiedSize { get; set; }
			public int TotalCount { get; }
			public long TotalSize { get; }

			public CopyStats(int totalCount, long totalSize, IProgress<ICopyStats> progress)
			{
				TotalCount = totalCount;
				TotalSize = totalSize;
				_progress = progress;
			}

			public void Update(int count, long size)
			{
				lock (_lockObject)
				{
					CopiedCount += count;
					CopiedSize += size;
					if (_timer.Elapsed > TimeSpan.FromSeconds(10.0))
					{
						_progress.Report(this);
						_timer.Restart();
					}
				}
			}

			public void Flush()
			{
				lock (_lockObject)
				{
					_progress.Report(this);
					_timer.Restart();
				}
			}
		}

		/// <summary>
		/// Adds files from a flat list of paths
		/// </summary>
		/// <param name="baseDir">Base directory to base paths relative to</param>
		/// <param name="files">Files to add</param>
		/// <param name="options">Options for chunking file content</param>
		/// <param name="writer">Writer for new node data</param>
		/// <param name="progress">Feedback interface for progress updates</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public static async Task<DirectoryNode> CreateAsync(DirectoryReference baseDir, IReadOnlyList<FileInfo> files, ChunkingOptions options, IStorageWriter writer, IProgress<ICopyStats>? progress, CancellationToken cancellationToken)
		{
			DirectoryNode directoryNode = new DirectoryNode();
			await directoryNode.AddFilesAsync(baseDir, files, options, writer, progress, cancellationToken);
			return directoryNode;
		}

		/// <inheritdoc cref="AddFilesAsync(DirectoryReference, IReadOnlyList{FileInfo}, ChunkingOptions, IStorageWriter, IProgress{ICopyStats}?, CancellationToken)"/>
		public Task AddFilesAsync(DirectoryReference baseDir, IEnumerable<FileReference> files, ChunkingOptions options, IStorageWriter writer, IProgress<ICopyStats>? progress, CancellationToken cancellationToken)
		{
			return AddFilesAsync(baseDir, files.Select(x => x.ToFileInfo()).ToList(), options, writer, progress, cancellationToken);
		}

		/// <inheritdoc cref="AddFilesAsync(DirectoryReference, IReadOnlyList{FileInfo}, ChunkingOptions, IStorageWriter, IProgress{ICopyStats}?, CancellationToken)"/>
		public Task AddFilesAsync(DirectoryInfo baseDir, IEnumerable<FileInfo> files, ChunkingOptions options, IStorageWriter writer, IProgress<ICopyStats>? progress, CancellationToken cancellationToken)
		{
			return AddFilesAsync(baseDir, files.ToList(), options, writer, progress, cancellationToken);
		}

		/// <summary>
		/// Adds files from a directory to the storage
		/// </summary>
		/// <param name="baseDir">Base directory to base paths relative to</param>
		/// <param name="files">Files to add</param>
		/// <param name="options">Options for chunking file content</param>
		/// <param name="writer">Writer for new node data</param>
		/// <param name="progress">Feedback interface for progress updates</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public async Task AddFilesAsync(DirectoryReference baseDir, IReadOnlyList<FileInfo> files, ChunkingOptions options, IStorageWriter writer, IProgress<ICopyStats>? progress, CancellationToken cancellationToken)
		{
			// Get the total size of all the files
			long totalSize = 0;
			foreach (FileInfo file in files)
			{
				totalSize += file.Length;
			}

			CopyStats? copyStats = null;
			if (progress != null)
			{
				copyStats = new CopyStats(files.Count, totalSize, progress);
			}

			// Partition them into blocks for parallel writers to process asynchronously
			List<(int Start, int Count)> partitions = ComputePartitions(files, totalSize);
			List<NodeRef<ChunkedDataNode>>[] leafChunks = new List<NodeRef<ChunkedDataNode>>[files.Count];
			await Parallel.ForEachAsync(partitions, cancellationToken, (filePartition, ctx) => CreateLeafChunkNodesAsync(writer, files, leafChunks, filePartition.Start, filePartition.Count, copyStats, options, cancellationToken));

			// Create interior nodes for all the leaf chunks
			NodeRef<ChunkedDataNode>[] chunks = await CreateInteriorChunkNodesAsync(leafChunks, options.InteriorOptions, writer, cancellationToken);

			// Write all the interior nodes and generate the directory update
			DirectoryUpdate update = new DirectoryUpdate();
			for(int idx = 0; idx < files.Count; idx++)
			{
				FileInfo file = files[idx];
				FileEntry entry = new FileEntry(file.Name, FileEntryFlags.None, file.Length, chunks[idx]);
				update.AddFile(new FileReference(file).MakeRelativeTo(baseDir), entry);
			}

			// Add all the new entries to the tree
			await UpdateAsync(update, writer, cancellationToken);
		}

		static List<(int Index, int Count)> ComputePartitions(IReadOnlyList<FileInfo> files, long totalSize)
		{
			// Maximum number of streams to write in parallel
			const int MaxPartitions = 8;

			// Minimum size of output payload for each writer
			const long MinSizePerPartition = 1024 * 1024;

			// Calculate the number of writers
			int numPartitions = 1 + (int)Math.Min(totalSize / MinSizePerPartition, MaxPartitions);

			// Create the partitions
			List<(int Index, int Count)> partitions = new List<(int, int)>();

			long writtenSize = 0;
			long remainingSizeForPartition = 0;

			int startIndex = 0;
			for (int index = 0; index < files.Count; index++)
			{
				FileInfo file = files[index];
				if (file.Length >= remainingSizeForPartition && partitions.Count < numPartitions)
				{
					remainingSizeForPartition = (totalSize - writtenSize) / (numPartitions - partitions.Count);
					partitions.Add((startIndex, index - startIndex));
					startIndex = index;
				}

				writtenSize += file.Length;
				remainingSizeForPartition -= file.Length;
			}

			partitions.Add((startIndex, files.Count - startIndex));
			return partitions;
		}

		static async ValueTask CreateLeafChunkNodesAsync(IStorageWriter writer, IReadOnlyList<FileInfo> files, List<NodeRef<ChunkedDataNode>>[] leafChunks, int start, int count, CopyStats? copyStats, ChunkingOptions options, CancellationToken cancellationToken)
		{
			await using IStorageWriter writerFork = writer.Fork();
			for (int idx = start; idx < start + count; idx++)
			{
				FileInfo file = files[idx];
				using (Stream stream = file.OpenRead())
				{
					leafChunks[idx] = await LeafChunkedDataNode.CreateFromStreamAsync(writerFork, stream, options.LeafOptions, cancellationToken);
				}
				copyStats?.Update(1, file.Length);
			}
			await writerFork.FlushAsync(cancellationToken);
		}

		static async Task<NodeRef<ChunkedDataNode>[]> CreateInteriorChunkNodesAsync(List<NodeRef<ChunkedDataNode>>[] chunks, InteriorChunkedDataNodeOptions options, IStorageWriter writer, CancellationToken cancellationToken)
		{
			NodeRef<ChunkedDataNode>[] nodes = new NodeRef<ChunkedDataNode>[chunks.Length];
			for (int idx = 0; idx < chunks.Length; idx++)
			{
				nodes[idx] = await InteriorChunkedDataNode.CreateTreeAsync(chunks[idx], options, writer, cancellationToken);
			}
			return nodes;
		}

		/// <summary>
		/// Updates this tree of directory objects
		/// </summary>
		/// <param name="updates">Files to add</param>
		/// <param name="writer">Writer for new node data</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public Task UpdateAsync(IEnumerable<FileUpdate> updates, IStorageWriter writer, CancellationToken cancellationToken = default)
		{
			DirectoryUpdate update = new DirectoryUpdate();
			update.AddFiles(updates);
			return UpdateAsync(update, writer, cancellationToken);
		}

		/// <summary>
		/// Updates this tree of directory objects
		/// </summary>
		/// <param name="update">Files to add</param>
		/// <param name="writer">Writer for new node data</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public async Task UpdateAsync(DirectoryUpdate update, IStorageWriter writer, CancellationToken cancellationToken = default)
		{
			foreach ((string name, DirectoryUpdate? directory) in update.Directories)
			{
				if (directory == null)
				{
					_nameToDirectoryEntry.Remove(name);
				}
				else
				{
					DirectoryNode? childNode = await FindDirectoryAsync(name, cancellationToken);
					childNode ??= new DirectoryNode();
					await childNode.UpdateAsync(directory, writer, cancellationToken);
					NodeRef<DirectoryNode> nodeRef = await writer.WriteNodeAsync(childNode, cancellationToken);
					_nameToDirectoryEntry[name] = new DirectoryEntry(name, childNode.Length, nodeRef);
				}
			}
			foreach ((string name, FileEntry? file) in update.Files)
			{
				if (file == null)
				{
					_nameToFileEntry.Remove(name);
				}
				else
				{
					_nameToFileEntry[name] = file;
				}
			}
		}

		/// <summary>
		/// Copies entries from a zip file
		/// </summary>
		/// <param name="stream">Input stream</param>
		/// <param name="writer">Writer for new nodes</param>
		/// <param name="options"></param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public async Task CopyFromZipStreamAsync(Stream stream, IStorageWriter writer, ChunkingOptions options, CancellationToken cancellationToken = default)
		{
			// Create all the leaf nodes
			List<(ZipArchiveEntry, List<NodeRef<ChunkedDataNode>>)> entries = new List<(ZipArchiveEntry, List<NodeRef<ChunkedDataNode>>)>();
			using (ZipArchive archive = new ZipArchive(stream, ZipArchiveMode.Read, true))
			{
				foreach (ZipArchiveEntry entry in archive.Entries)
				{
					if (entry.Name.Length > 0)
					{
						using (Stream entryStream = entry.Open())
						{
							List<NodeRef<ChunkedDataNode>> chunks = await LeafChunkedDataNode.CreateFromStreamAsync(writer, entryStream, options.LeafOptions, cancellationToken);
							entries.Add((entry, chunks));
						}
					}
				}
			}

			// Create all the interior nodes
			List<FileUpdate> updates = new List<FileUpdate>();
			foreach ((ZipArchiveEntry entry, List<NodeRef<ChunkedDataNode>> chunks) in entries)
			{
				FileEntryFlags flags = FileEntryFlags.None;
				if ((entry.ExternalAttributes & (0b_001_001_001 << 16)) != 0)
				{
					flags |= FileEntryFlags.Executable;
				}

				NodeRef<ChunkedDataNode> chunk = await InteriorChunkedDataNode.CreateTreeAsync(chunks, options.InteriorOptions, writer, cancellationToken);
				updates.Add(new FileUpdate(entry.FullName, flags, entry.Length, chunk));
			}

			// Update the tree
			await UpdateAsync(updates, writer, cancellationToken);
		}

		/// <summary>
		/// Adds files from a directory on disk
		/// </summary>
		/// <param name="directoryInfo"></param>
		/// <param name="options">Options for chunking file content</param>
		/// <param name="writer">Writer for new node data</param>
		/// <param name="progress">Feedback interface for progress updates</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public async Task CopyFromDirectoryAsync(DirectoryInfo directoryInfo, ChunkingOptions options, IStorageWriter writer, IProgress<ICopyStats>? progress, CancellationToken cancellationToken = default)
		{
			await AddFilesAsync(new DirectoryReference(directoryInfo), directoryInfo.EnumerateFiles("*", SearchOption.AllDirectories).ToList(), options, writer, progress, cancellationToken);
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
			foreach (FileEntry fileEntry in _nameToFileEntry.Values)
			{
				FileInfo fileInfo = new FileInfo(Path.Combine(directoryInfo.FullName, fileEntry.Name.ToString()));
				ChunkedDataNode fileNode = await fileEntry.ExpandAsync(cancellationToken);
				tasks.Add(Task.Run(() => fileNode.CopyToFileAsync(fileInfo, cancellationToken), cancellationToken));
			}
			foreach (DirectoryEntry directoryEntry in _nameToDirectoryEntry.Values)
			{
				DirectoryInfo subDirectoryInfo = directoryInfo.CreateSubdirectory(directoryEntry.Name.ToString());
				DirectoryNode subDirectoryNode = await directoryEntry.ExpandAsync(cancellationToken);
				tasks.Add(Task.Run(() => subDirectoryNode.CopyToDirectoryAsync(subDirectoryInfo, logger, cancellationToken), cancellationToken));
			}

			await Task.WhenAll(tasks);
		}

		/// <summary>
		/// Returns a stream containing the zipped contents of this directory
		/// </summary>
		/// <param name="filter">Filter for files to include in the zip</param>
		/// <returns>Stream containing zipped archive data</returns>
		public Stream AsZipStream(FileFilter? filter = null) => new DirectoryNodeZipStream(this, filter);
	}

	/// <summary>
	/// Describes an update to a file in a directory tree
	/// </summary>
	/// <param name="Path">Path to the file</param>
	/// <param name="Length">Length of the file data</param>
	/// <param name="Flags">Flags for the new file entry</param>
	/// <param name="DataRef">Reference to the root data node</param>
	public record class FileUpdate(string Path, FileEntryFlags Flags, long Length, NodeRef<ChunkedDataNode> DataRef);

	/// <summary>
	/// Describes an update to a directory node
	/// </summary>
	public class DirectoryUpdate
	{
		/// <summary>
		/// Directories to be updated
		/// </summary>
		public Dictionary<string, DirectoryUpdate?> Directories { get; } = new Dictionary<string, DirectoryUpdate?>();

		/// <summary>
		/// Files to be updated
		/// </summary>
		public Dictionary<string, FileEntry?> Files { get; } = new Dictionary<string, FileEntry?>();

		/// <summary>
		/// Reset this instance
		/// </summary>
		public void Clear()
		{
			Directories.Clear();
			Files.Clear();
		}

		/// <summary>
		/// Adds a file by path to this object
		/// </summary>
		/// <param name="path">Path to add to</param>
		/// <param name="fileEntry">Content for the file</param>
		public void AddFile(string path, FileEntry? fileEntry)
		{
			string[] fragments = path.Split(new char[] { '/', '\\' });

			DirectoryUpdate lastDir = this;
			for (int idx = 0; idx + 1 < fragments.Length; idx++)
			{
				DirectoryUpdate? nextDir;
				if (!lastDir.Directories.TryGetValue(fragments[idx], out nextDir) || nextDir == null)
				{
					if (fileEntry == null)
					{
						return;
					}

					nextDir = new DirectoryUpdate();
					lastDir.Directories.Add(fragments[idx], nextDir);
				}
				lastDir = nextDir;
			}

			lastDir.Files[fragments[^1]] = fileEntry;
		}

		/// <summary>
		/// Adds a file to the tree
		/// </summary>
		/// <param name="path">Path to the file</param>
		/// <param name="flags">Flags for the new file entry</param>
		/// <param name="length">Length of the file</param>
		/// <param name="dataRef">Reference to the file data</param>
		public FileEntry AddFile(string path, FileEntryFlags flags, long length, NodeRef<ChunkedDataNode> dataRef)
		{
			string name = path;
			for (int idx = path.Length - 1; idx >= 0; idx--)
			{
				if (path[idx] == Path.DirectorySeparatorChar || path[idx] == Path.AltDirectorySeparatorChar)
				{
					name = path.Substring(idx + 1);
					break;
				}
			}

			FileEntry entry = new FileEntry(name, flags, length, dataRef);
			AddFile(path, entry);
			return entry;
		}

		/// <summary>
		/// Adds a filtered list of files from disk
		/// </summary>
		/// <param name="files">Files to add</param>
		public void AddFiles(IEnumerable<FileUpdate> files)
		{
			IEnumerator<FileUpdate> enumerator = files.GetEnumerator();
			if (enumerator.MoveNext())
			{
				AddFiles(String.Empty, enumerator);
			}
		}

		bool AddFiles(ReadOnlySpan<char> prefix, IEnumerator<FileUpdate> files)
		{
			for (; ; )
			{
				FileUpdate fileUpdate = files.Current;
				if (fileUpdate.Path.Length < prefix.Length || !fileUpdate.Path.AsSpan().StartsWith(prefix, FileReference.Comparison))
				{
					return true;
				}

				int nextDirLength = fileUpdate.Path.AsSpan(prefix.Length).IndexOfAny(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
				if (nextDirLength == -1)
				{
					string fileName = fileUpdate.Path.Substring(prefix.Length);
					Files[fileName] = new FileEntry(fileName, fileUpdate.Flags, fileUpdate.Length, fileUpdate.DataRef);

					if (!files.MoveNext())
					{
						return false;
					}
				}
				else
				{
					string dirName = fileUpdate.Path.Substring(prefix.Length, nextDirLength);

					DirectoryUpdate? nextTree;
					if (!Directories.TryGetValue(dirName, out nextTree) || nextTree == null)
					{
						nextTree = new DirectoryUpdate();
						Directories[dirName] = nextTree;
					}

					ReadOnlySpan<char> nextPrefix = fileUpdate.Path.AsSpan(0, prefix.Length + nextDirLength + 1);
					if (!nextTree.AddFiles(nextPrefix, files))
					{
						return false;
					}
				}
			}
		}
	}

	/// <summary>
	/// Stream which zips a directory node tree dynamically
	/// </summary>
	class DirectoryNodeZipStream : Stream
	{
		/// <inheritdoc/>
		public override bool CanRead => true;

		/// <inheritdoc/>
		public override bool CanSeek => false;

		/// <inheritdoc/>
		public override bool CanWrite => false;

		/// <inheritdoc/>
		public override long Length => throw new NotImplementedException();

		/// <inheritdoc/>
		public override long Position { get => _position; set => throw new NotImplementedException(); }

		readonly Pipe _pipe;
		readonly BackgroundTask _backgroundTask;

		long _position;
		ReadOnlySequence<byte> _current = ReadOnlySequence<byte>.Empty;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="node">Root node to copy from</param>
		/// <param name="filter">Filter for files to include in the zip</param>
		public DirectoryNodeZipStream(DirectoryNode node, FileFilter? filter)
		{
			_pipe = new Pipe();
			_backgroundTask = BackgroundTask.StartNew(ctx => CopyToPipeAsync(node, filter, _pipe.Writer, ctx));
		}

		/// <inheritdoc/>
		public override async ValueTask DisposeAsync()
		{
			await base.DisposeAsync();

			await _backgroundTask.DisposeAsync();
		}

		/// <inheritdoc/>
		public override async ValueTask<int> ReadAsync(Memory<byte> buffer, CancellationToken cancellationToken = default)
		{
			while (_current.Length == 0)
			{
				ReadResult result = await _pipe.Reader.ReadAsync(cancellationToken);
				_current = result.Buffer;

				if (result.IsCompleted && _current.Length == 0)
				{
					return 0;
				}
			}

			int initialSize = buffer.Length;
			while (buffer.Length > 0 && _current.Length > 0)
			{
				int copy = Math.Min(buffer.Length, _current.First.Length);
				_current.First.Slice(0, copy).CopyTo(buffer);
				_current = _current.Slice(copy);
				buffer = buffer.Slice(copy);
			}

			if (_current.Length == 0)
			{
				_pipe.Reader.AdvanceTo(_current.End);
			}

			int length = initialSize - buffer.Length;
			_position += length;
			return length;
		}

		static async Task CopyToPipeAsync(DirectoryNode node, FileFilter? filter, PipeWriter writer, CancellationToken cancellationToken)
		{
			using Stream outputStream = writer.AsStream();
			using ZipArchive archive = new ZipArchive(outputStream, ZipArchiveMode.Create);
			await CopyFilesAsync(node, "", filter, archive, cancellationToken);
		}

		static async Task CopyFilesAsync(DirectoryNode directory, string prefix, FileFilter? filter, ZipArchive archive, CancellationToken cancellationToken)
		{
			foreach (DirectoryEntry directoryEntry in directory.Directories)
			{
				string directoryPath = $"{prefix}{directoryEntry.Name}/";
				if (filter == null || filter.PossiblyMatches(directoryPath))
				{
					DirectoryNode node = await directoryEntry.ExpandAsync(cancellationToken);
					await CopyFilesAsync(node, directoryPath, filter, archive, cancellationToken);
				}
			}

			foreach (FileEntry fileEntry in directory.Files)
			{
				string filePath = $"{prefix}{fileEntry}";
				if (filter == null || filter.Matches(filePath))
				{
					ZipArchiveEntry entry = archive.CreateEntry(filePath);

					if ((fileEntry.Flags & FileEntryFlags.Executable) != 0)
					{
						entry.ExternalAttributes |= 0b_111_111_101 << 16; // rwx rwx r-x
					}
					else
					{
						entry.ExternalAttributes |= 0b_110_110_100 << 16; // rw- rw- r--
					}

					using Stream entryStream = entry.Open();
					await fileEntry.CopyToStreamAsync(entryStream, cancellationToken);
				}
			}
		}

		/// <inheritdoc/>
		public override void Flush()
		{
		}

		/// <inheritdoc/>
		public override int Read(byte[] buffer, int offset, int count) => ReadAsync(buffer.AsMemory(offset, count)).AsTask().Result;

		/// <inheritdoc/>
		public override long Seek(long offset, SeekOrigin origin) => throw new NotSupportedException();

		/// <inheritdoc/>
		public override void SetLength(long value) => throw new NotSupportedException();

		/// <inheritdoc/>
		public override void Write(byte[] buffer, int offset, int count) => throw new NotSupportedException();
	}
}
