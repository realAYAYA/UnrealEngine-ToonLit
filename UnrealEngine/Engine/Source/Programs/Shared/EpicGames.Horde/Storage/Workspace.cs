// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage.Nodes;
using Microsoft.Extensions.Logging;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Provides functionality to extract and patch data in a local workspace
	/// </summary>
	public class Workspace
	{
		// Tracked state of a directory
		class DirectoryState
		{
			public DirectoryState? Parent { get; }
			public string Name { get; }
			public List<DirectoryState> Directories { get; } = new List<DirectoryState>();
			public List<FileState> Files { get; } = new List<FileState>();

			public ulong LayerFlags { get; set; }

			public DirectoryState(DirectoryState? parent, string name)
			{
				Parent = parent;
				Name = name;
			}

			public void AddFile(FileState fileState)
			{
				int index = Files.BinarySearch(x => x.Name, fileState.Name, StringComparer.Ordinal);
				if (index >= 0)
				{
					throw new InvalidOperationException($"File {fileState.Name} already exists");
				}

				fileState.Parent = this;
				Files.Insert(~index, fileState);
			}

			public void RemoveFile(FileState fileState)
			{
				int index = Files.BinarySearch(x => x.Name, fileState.Name);
				if (index >= 0 && Files[index] == fileState)
				{
					Files.RemoveAt(index);
				}
			}

			public bool ContainsFile(string name) => TryGetFile(name, out _);

			public bool TryGetFile(string name, [NotNullWhen(true)] out FileState? fileState)
			{
				int index = Files.BinarySearch(x => x.Name, name, StringComparer.Ordinal);
				if (index >= 0)
				{
					fileState = Files[index];
					return true;
				}
				else
				{
					fileState = null;
					return false;
				}
			}

			public FileState FindOrAddFile(string name)
			{
				int index = Files.BinarySearch(x => x.Name, name, StringComparer.Ordinal);
				if (index >= 0)
				{
					return Files[index];
				}
				else
				{
					FileState fileState = new FileState(this, name);
					Files.Insert(~index, fileState);
					return fileState;
				}
			}

			public bool TryGetDirectory(string name, [NotNullWhen(true)] out DirectoryState? directoryState)
			{
				int index = Directories.BinarySearch(x => x.Name, name, StringComparer.Ordinal);
				if (index >= 0)
				{
					directoryState = Directories[index];
					return true;
				}
				else
				{
					directoryState = null;
					return false;
				}
			}

			public DirectoryState FindOrAddDirectory(string name)
			{
				int index = Directories.BinarySearch(x => x.Name, name, StringComparer.Ordinal);
				if (index >= 0)
				{
					return Directories[index];
				}
				else
				{
					DirectoryState subDirState = new DirectoryState(this, name);
					Directories.Insert(~index, subDirState);
					return subDirState;
				}
			}

			public string GetPath()
			{
				StringBuilder builder = new StringBuilder();
				AppendPath(builder);
				return builder.ToString();
			}

			public void AppendPath(StringBuilder builder)
			{
				Parent?.AppendPath(builder);

				if (Name.Length > 0)
				{
					builder.Append(Name);
				}
				if (builder.Length == 0 || builder[^1] != Path.DirectorySeparatorChar)
				{
					builder.Append(Path.DirectorySeparatorChar);
				}
			}

			public DirectoryReference GetDirectoryReference(DirectoryReference baseDir)
			{
				StringBuilder builder = new StringBuilder(baseDir.FullName);
				AppendPath(builder);
				return new DirectoryReference(builder.ToString(), DirectoryReference.Sanitize.None);
			}

			public void Read(IMemoryReader reader)
			{
				int numDirectories = reader.ReadInt32();
				Directories.Capacity = numDirectories;
				Directories.Clear();

				for (int idx = 0; idx < numDirectories; idx++)
				{
					string subDirName = reader.ReadString();

					DirectoryState subDirState = new DirectoryState(this, subDirName);
					subDirState.Read(reader);

					Directories.Add(subDirState);
				}

				int numFiles = reader.ReadInt32();
				Files.Capacity = numFiles;
				Files.Clear();

				for (int idx = 0; idx < numFiles; idx++)
				{
					string fileName = reader.ReadString();

					FileState fileState = new FileState(this, fileName);
					fileState.Read(reader);

					Files.Add(fileState);
				}

				LayerFlags = reader.ReadUnsignedVarInt();
			}

			public void Write(IMemoryWriter writer)
			{
				writer.WriteInt32(Directories.Count);
				foreach (DirectoryState directory in Directories)
				{
					writer.WriteString(directory.Name);
					directory.Write(writer);
				}

				writer.WriteInt32(Files.Count);
				foreach (FileState file in Files)
				{
					writer.WriteString(file.Name);
					file.Write(writer);
				}

				writer.WriteUnsignedVarInt(LayerFlags);
			}

			public override string ToString() => GetPath();
		}

		// Tracked state of a file
		[DebuggerDisplay("{Name}")]
		class FileState
		{
			public DirectoryState Parent { get; set; }
			public string Name { get; private set; }
			public long Length { get; private set; }
			public long LastModifiedTimeUtc { get; private set; }
			public IoHash Hash { get; set; }
			public ulong LayerFlags { get; set; }

			public FileState(DirectoryState parent, string name)
			{
				Parent = parent;
				Name = name;
			}

			public void Read(IMemoryReader reader)
			{
				Length = reader.ReadInt64();
				LastModifiedTimeUtc = reader.ReadInt64();
				Hash = reader.ReadIoHash();

				LayerFlags = reader.ReadUnsignedVarInt();
			}

			public void MoveTo(DirectoryState newParent, string newName)
			{
				Parent.RemoveFile(this);
				Name = newName;
				newParent.AddFile(this);
			}

			public void Delete()
			{
				Parent.RemoveFile(this);
			}

			public bool IsModified(FileInfo fileInfo) => Length != fileInfo.Length || LastModifiedTimeUtc != fileInfo.LastWriteTimeUtc.Ticks;

			public void Update(FileInfo fileInfo)
			{
				Length = fileInfo.Length;
				LastModifiedTimeUtc = fileInfo.LastWriteTimeUtc.Ticks;
			}

			public void Write(IMemoryWriter writer)
			{
				Debug.Assert(Hash != IoHash.Zero);
				writer.WriteInt64(Length);
				writer.WriteInt64(LastModifiedTimeUtc);
				writer.WriteIoHash(Hash);
				writer.WriteUnsignedVarInt(LayerFlags);
			}

			public string GetPath()
			{
				StringBuilder builder = new StringBuilder();
				AppendPath(builder);
				return builder.ToString();
			}

			public void AppendPath(StringBuilder builder)
			{
				Parent.AppendPath(builder);
				builder.Append(Name);
			}

			public FileReference GetFileReference(DirectoryReference baseDir)
			{
				StringBuilder builder = new StringBuilder(baseDir.FullName);
				AppendPath(builder);
				return new FileReference(builder.ToString(), FileReference.Sanitize.None);
			}

			public override string ToString() => GetPath();
		}

		// Collates lists of files and chunks with a particular hash
		[DebuggerDisplay("{Hash}")]
		class HashInfo
		{
			public int Index { get; set; }

			public IoHash Hash { get; }
			public long Length { get; }

			public List<FileState> Files { get; } = new List<FileState>();
			public List<ChunkInfo> Chunks { get; } = new List<ChunkInfo>();

			public HashInfo(IoHash hash, long length)
			{
				Hash = hash;
				Length = length;
			}
		}

		// Hashed chunk within another hashed object
		[DebuggerDisplay("{Offset}+{Length}")]
		class ChunkInfo
		{
			public HashInfo WithinHashInfo { get; }
			public long Offset { get; }
			public long Length { get; }

			public ChunkInfo(HashInfo withinHashInfo, long offset, long length)
			{
				WithinHashInfo = withinHashInfo;
				Offset = offset;
				Length = length;
			}

			public ChunkInfo(IMemoryReader reader, HashInfo[] hashes)
			{
				WithinHashInfo = hashes[(int)reader.ReadUnsignedVarInt()];
				Offset = (long)reader.ReadUnsignedVarInt();
				Length = (long)reader.ReadUnsignedVarInt();
			}

			public void Write(IMemoryWriter writer)
			{
				writer.WriteUnsignedVarInt(WithinHashInfo.Index);
				writer.WriteUnsignedVarInt((ulong)Offset);
				writer.WriteUnsignedVarInt((ulong)Length);
			}
		}

		// Maps a layer id to a flag
		class LayerState
		{
			public WorkspaceLayerId Id { get; }
			public ulong Flag { get; }

			public LayerState(WorkspaceLayerId id, ulong flag)
			{
				Id = id;
				Flag = flag;
			}

			public LayerState(IMemoryReader reader)
			{
				Id = new WorkspaceLayerId(new StringId(reader.ReadUtf8String()));
				Flag = reader.ReadUnsignedVarInt();
			}

			public void Write(IMemoryWriter writer)
			{
				writer.WriteUtf8String(Id.Id.Text);
				writer.WriteUnsignedVarInt(Flag);
			}
		}

		readonly DirectoryReference _rootDir;
		readonly FileReference _stateFile;
		readonly DirectoryState _rootDirState;
		readonly List<LayerState> _layers;
		readonly Dictionary<IoHash, HashInfo> _hashes = new Dictionary<IoHash, HashInfo>();
		readonly ILogger _logger;

		const string HordeDirName = ".horde";
		const string CacheDirName = "cache";
		const string StateFileName = "contents.dat";

		/// <summary>
		/// Flag for the default layer
		/// </summary>
		const ulong DefaultLayerFlag = 1;

		/// <summary>
		/// Flag for the cache layer
		/// </summary>
		const ulong CacheLayerFlag = 2;

		/// <summary>
		/// Flags for user layers
		/// </summary>
		const ulong ReservedLayerFlags = DefaultLayerFlag | CacheLayerFlag;

		/// <summary>
		/// Root directory for the workspace
		/// </summary>
		public DirectoryReference RootDir => _rootDir;

		/// <summary>
		/// Layers current in this workspace
		/// </summary>
		public IReadOnlyList<WorkspaceLayerId> Layers => _layers.Select(x => x.Id).ToList();

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="rootDir">Directory for the workspace</param>
		/// <param name="stateFile">Path to the state file for this directory</param>
		/// <param name="logger">Logger for diagnostic output</param>
		private Workspace(DirectoryReference rootDir, FileReference stateFile, ILogger logger)
		{
			_rootDir = rootDir;
			_stateFile = stateFile;
			_rootDirState = new DirectoryState(null, String.Empty);
			_layers = new List<LayerState> { new LayerState(WorkspaceLayerId.Default, 1) };
			_logger = logger;
		}

		/// <summary>
		/// Create a new workspace instance in the given location. Opens the existing instance if it already contains workspace data.
		/// </summary>
		/// <param name="rootDir">Root directory for the workspace</param>
		/// <param name="logger">Logger for output</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Workspace instance</returns>
		public static async Task<Workspace> CreateAsync(DirectoryReference rootDir, ILogger logger, CancellationToken cancellationToken = default)
		{
			FileReference stateFile = FileReference.Combine(rootDir, HordeDirName, StateFileName);

			using FileStream? stream = FileTransaction.OpenRead(stateFile);
			if (stream != null)
			{
				throw new InvalidOperationException($"Workspace already exists in {rootDir}; use Open instead.");
			}

			Workspace workspace = new Workspace(rootDir, stateFile, logger);
			await workspace.SaveAsync(cancellationToken);

			return workspace;
		}

		/// <summary>
		/// Attempts to open an existing workspace for the current directory. 
		/// </summary>
		/// <param name="currentDir">Root directory for the workspace</param>
		/// <param name="logger">Logger for output</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Workspace instance</returns>
		public static async Task<Workspace?> TryOpenAsync(DirectoryReference currentDir, ILogger logger, CancellationToken cancellationToken = default)
		{
			for (DirectoryReference? testDir = currentDir; testDir != null; testDir = testDir.ParentDirectory)
			{
				FileReference stateFile = FileReference.Combine(testDir, HordeDirName, StateFileName);
				using (FileStream? stream = FileTransaction.OpenRead(stateFile))
				{
					if (stream != null)
					{
						byte[] data = await stream.ReadAllBytesAsync(cancellationToken);

						Workspace workspace = new Workspace(testDir, stateFile, logger);
						workspace.Read(new MemoryReader(data));

						return workspace;
					}
				}
			}
			return null;
		}

		/// <summary>
		/// Save the current state of the workspace
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public async Task SaveAsync(CancellationToken cancellationToken)
		{
			DirectoryReference.CreateDirectory(_stateFile.Directory);

			using (FileTransactionStream stream = FileTransaction.OpenWrite(_stateFile))
			{
				using (ChunkedMemoryWriter writer = new ChunkedMemoryWriter(64 * 1024))
				{
					Write(writer);
					await writer.CopyToAsync(stream, cancellationToken);
				}
				stream.CompleteTransaction();
			}
		}

		void Read(IMemoryReader reader)
		{
			_rootDirState.Read(reader);
			reader.ReadList(_layers, () => new LayerState(reader));

			// Read the hash lookup
			int numHashes = reader.ReadInt32();
			HashInfo[] hashInfoArray = new HashInfo[numHashes];

			_hashes.EnsureCapacity(numHashes);
			_hashes.Clear();

			for (int idx = 0; idx < numHashes; idx++)
			{
				IoHash hash = reader.ReadIoHash();
				long length = (long)reader.ReadUnsignedVarInt();
				hashInfoArray[idx] = new HashInfo(hash, length);
				_hashes.Add(hash, hashInfoArray[idx]);
			}
			for (int idx = 0; idx < numHashes; idx++)
			{
				HashInfo hashInfo = hashInfoArray[idx];
				reader.ReadList(hashInfoArray[idx].Chunks, () => new ChunkInfo(reader, hashInfoArray));
			}

			// Add all the files to the hash lookup
			AddDirToHashLookup(_rootDirState);
		}

		void Write(IMemoryWriter writer)
		{
			_rootDirState.Write(writer);
			writer.WriteList(_layers, x => x.Write(writer));

			// Write the hash lookup
			writer.WriteInt32(_hashes.Count);

			int nextIndex = 0;
			foreach (HashInfo hashInfo in _hashes.Values)
			{
				writer.WriteIoHash(hashInfo.Hash);
				writer.WriteUnsignedVarInt((ulong)hashInfo.Length);
				hashInfo.Index = nextIndex++;
			}
			foreach (HashInfo hashInfo in _hashes.Values)
			{
				writer.WriteList(hashInfo.Chunks, x => x.Write(writer));
			}
		}

		#region Layers

		/// <summary>
		/// Add or update a layer with the given identifier
		/// </summary>
		/// <param name="id">Identifier for the layer</param>
		public void AddLayer(WorkspaceLayerId id)
		{
			if (_layers.Any(x => x.Id == id))
			{
				throw new InvalidOperationException($"Layer {id} already exists");
			}

			ulong flags = ReservedLayerFlags;
			for (int idx = 0; idx < _layers.Count; idx++)
			{
				flags |= _layers[idx].Flag;
			}
			if (flags == ~0UL)
			{
				throw new InvalidOperationException("Maximum number of layers reached");
			}

			ulong nextFlag = (flags + 1) ^ flags;
			_layers.Add(new LayerState(id, nextFlag));
		}

		/// <summary>
		/// Removes a layer with the given identifier. Does not remove any files in the workspace.
		/// </summary>
		/// <param name="layerId">Layer to update</param>
		public void RemoveLayer(WorkspaceLayerId layerId)
		{
			int layerIdx = _layers.FindIndex(x => x.Id == layerId);
			if (layerIdx > 0) // Note: Excluding default layer at index 0
			{
				LayerState layer = _layers[layerIdx];
				if ((_rootDirState.LayerFlags & layer.Flag) != 0)
				{
					throw new InvalidOperationException($"Workspace still contains files for layer {layerId}");
				}
				_layers.RemoveAt(layerIdx);
			}
		}

		LayerState? GetLayerState(WorkspaceLayerId layerId) => _layers.FirstOrDefault(x => x.Id == layerId);

		#endregion

		#region Syncing

		/// <summary>
		/// Syncs a layer to the given contents
		/// </summary>
		/// <param name="layerId">Identifier for the layer</param>
		/// <param name="contents">New contents for the layer</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public async Task SyncAsync(WorkspaceLayerId layerId, DirectoryNode? contents, CancellationToken cancellationToken = default)
		{
			LayerState layerState = GetLayerState(layerId) ?? throw new InvalidOperationException($"Layer '{layerId}' does not exist");

			DirectoryState hordeDirState = _rootDirState.FindOrAddDirectory(HordeDirName);
			hordeDirState.LayerFlags |= CacheLayerFlag;

			DirectoryState cacheDirState = hordeDirState.FindOrAddDirectory(CacheDirName);
			cacheDirState.LayerFlags |= CacheLayerFlag;

			await SyncDirectoryAsync(_rootDir, _rootDirState, contents, layerState.Flag, cacheDirState, cancellationToken);
		}

		async Task SyncDirectoryAsync(DirectoryReference dirRef, DirectoryState dirState, DirectoryNode? dirNode, ulong flag, DirectoryState cacheDirState, CancellationToken cancellationToken)
		{
			// Remove any directories that no longer exist
			for (int subDirIdx = 0; subDirIdx < dirState.Directories.Count; subDirIdx++)
			{
				DirectoryState subDirState = dirState.Directories[subDirIdx];
				if ((subDirState.LayerFlags & flag) != 0)
				{
					if (dirNode == null || !dirNode.TryGetDirectoryEntry(subDirState.Name, out _))
					{
						DirectoryReference subDirPath = DirectoryReference.Combine(dirRef, subDirState.Name.ToString());
						await SyncDirectoryAsync(subDirPath, subDirState, null, flag, cacheDirState, cancellationToken);

						if (subDirState.LayerFlags == 0)
						{
							dirState.Directories.Remove(subDirState);
							subDirIdx--;
						}
					}
				}
			}

			// Remove any files that no longer exist
			for (int fileIdx = 0; fileIdx < dirState.Files.Count; fileIdx++)
			{
				FileState fileState = dirState.Files[fileIdx];
				if ((fileState.LayerFlags & flag) != 0)
				{
					if (dirNode == null || !dirNode.TryGetFileEntry(fileState.Name, out FileEntry? entry) || entry.StreamHash != fileState.Hash)
					{
						fileState.LayerFlags &= ~flag;

						if (fileState.LayerFlags == 0)
						{
							MoveFileToCache(fileState, cacheDirState);
							fileIdx--;
						}
					}
				}
			}

			// Clear out the layer flag for this directory. It'll be added back if we add/reuse files below.
			dirState.LayerFlags &= ~flag;

			// Add files for this directory
			if (dirNode != null)
			{
				DirectoryReference.CreateDirectory(dirRef);

				// Update directories
				foreach (DirectoryEntry subDirEntry in dirNode.Directories)
				{
					DirectoryReference subDirPath = DirectoryReference.Combine(dirRef, subDirEntry.Name.ToString());
					DirectoryState subDirState = dirState.FindOrAddDirectory(subDirEntry.Name);

					DirectoryNode subDirNode = await subDirEntry.Handle.ReadBlobAsync(cancellationToken: cancellationToken);
					await SyncDirectoryAsync(subDirPath, subDirState, subDirNode, flag, cacheDirState, cancellationToken);

					dirState.LayerFlags |= flag;
				}

				// Update files
				foreach (FileEntry fileEntry in dirNode.Files)
				{
					await SyncFileAsync(dirRef, dirState, fileEntry, flag, cancellationToken);
					dirState.LayerFlags |= flag;
				}
			}

			// Delete the directory if it's no longer needed
			if (dirState.LayerFlags == 0 && dirState.Parent != null)
			{
				FileUtils.ForceDeleteDirectory(dirRef);
			}
		}

		async Task SyncFileAsync(DirectoryReference dirRef, DirectoryState dirState, FileEntry fileEntry, ulong flag, CancellationToken cancellationToken)
		{
			FileState? fileState;
			if (dirState.TryGetFile(fileEntry.Name, out fileState))
			{
				if (fileState.Hash == fileEntry.StreamHash)
				{
					fileState.LayerFlags |= flag;
				}
				else
				{
					throw new InvalidOperationException($"Conflicting hash for file {fileState.GetFileReference(_rootDir)}");
				}
			}
			else
			{
				FileInfo fileInfo = FileReference.Combine(dirRef, fileEntry.Name.ToString()).ToFileInfo();

				fileState = TryMoveCachedData(fileEntry.StreamHash, dirState, fileEntry.Name);
				if (fileState == null)
				{
					fileState = dirState.FindOrAddFile(fileEntry.Name);

					_logger.LogInformation("Updating {File} to {Hash}", fileInfo, fileEntry.StreamHash);
					using (FileStream stream = fileInfo.Open(FileMode.Create, FileAccess.Write, FileShare.Read))
					{
						await ExtractDataAsync(fileEntry.Target, stream, cancellationToken);
						if (stream.Length != fileEntry.Length)
						{
							throw new EndOfStreamException($"Incorrect length for extracted file {fileInfo.FullName}");
						}
					}

					fileState.Hash = fileEntry.StreamHash;

					fileInfo.Refresh();
					fileState.Update(fileInfo);

					AddFileToHashLookup(fileState);
				}
				else
				{
					fileInfo.Refresh();
					fileState.Update(fileInfo);
				}

				fileState.LayerFlags |= flag;
			}
		}

		async Task ExtractDataAsync(ChunkedDataNodeRef nodeRef, Stream outputStream, CancellationToken cancellationToken)
		{
			if (nodeRef.Type == ChunkedDataNodeType.Leaf)
			{
				await ExtractLeafDataAsync(nodeRef, outputStream, cancellationToken);
			}
			else if (nodeRef.Type == ChunkedDataNodeType.Interior)
			{
				InteriorChunkedDataNode interiorNode = await nodeRef.Handle.ReadBlobAsync<InteriorChunkedDataNode>(cancellationToken: cancellationToken);
				foreach (ChunkedDataNodeRef childNodeRef in interiorNode.Children)
				{
					await ExtractDataAsync(childNodeRef, outputStream, cancellationToken);
				}
			}
			else
			{
				throw new InvalidDataException($"Invalid node type {nodeRef.Type}");
			}
		}

		async Task ExtractLeafDataAsync(ChunkedDataNodeRef nodeRef, Stream outputStream, CancellationToken cancellationToken)
		{
			// Try to copy cached data for this node
			HashInfo? hashInfo;
			if (_hashes.TryGetValue(nodeRef.Handle.Hash, out hashInfo))
			{
				if (await TryCopyCachedDataAsync(hashInfo, 0, hashInfo.Length, outputStream, cancellationToken))
				{
					return;
				}
			}

			// Otherwise 
			using BlobData blobData = await nodeRef.Handle.ReadBlobDataAsync(cancellationToken);
			await LeafChunkedDataNode.CopyToStreamAsync(blobData, outputStream, cancellationToken);
		}

		FileState? TryMoveCachedData(IoHash hash, DirectoryState targetDirState, string targetName)
		{
			HashInfo? hashInfo;
			if (_hashes.TryGetValue(hash, out hashInfo))
			{
				foreach (FileState file in hashInfo.Files)
				{
					if (file.LayerFlags == CacheLayerFlag)
					{
						MoveFile(file, targetDirState, targetName);
						file.LayerFlags = 0;
						return file;
					}
				}
			}
			return null;
		}

		async Task<bool> TryCopyCachedDataAsync(HashInfo hashInfo, long offset, long length, Stream outputStream, CancellationToken cancellationToken)
		{
			foreach (FileState file in hashInfo.Files)
			{
				if (await TryCopyChunkAsync(file, offset, length, outputStream, cancellationToken))
				{
					return true;
				}
			}
			foreach (ChunkInfo chunk in hashInfo.Chunks)
			{
				if (await TryCopyCachedDataAsync(chunk.WithinHashInfo, chunk.Offset, chunk.Length, outputStream, cancellationToken))
				{
					return true;
				}
			}
			return false;
		}

		async Task<bool> TryCopyChunkAsync(FileState file, long offset, long length, Stream outputStream, CancellationToken cancellationToken)
		{
			FileInfo fileInfo = GetFileInfo(file);
			if (fileInfo.Exists)
			{
				long initialPosition = outputStream.Position;

				using FileStream inputStream = fileInfo.OpenRead();
				inputStream.Seek(offset, SeekOrigin.Begin);

				byte[] tempBuffer = new byte[65536];
				while (length > 0)
				{
					int readSize = await inputStream.ReadAsync(tempBuffer.AsMemory(0, (int)Math.Min(length, tempBuffer.Length)), cancellationToken);
					if (readSize == 0)
					{
						outputStream.Seek(initialPosition, SeekOrigin.Begin);
						outputStream.SetLength(initialPosition);
						return false;
					}

					await outputStream.WriteAsync(tempBuffer.AsMemory(0, readSize), cancellationToken);
					length -= readSize;
				}
			}
			return true;
		}

		#endregion

		#region Verify

		/// <summary>
		/// Checks that all files within the workspace have the correct hash
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public async Task VerifyAsync(CancellationToken cancellationToken = default)
		{
			await VerifyAsync(_rootDirState, cancellationToken);
		}

		async Task VerifyAsync(DirectoryState dirState, CancellationToken cancellationToken)
		{
			foreach (DirectoryState subDirState in dirState.Directories)
			{
				await VerifyAsync(subDirState, cancellationToken);
			}
			foreach (FileState fileState in dirState.Files)
			{
				FileInfo fileInfo = GetFileInfo(fileState);
				if (fileState.IsModified(fileInfo))
				{
					throw new InvalidDataException($"File {fileInfo.FullName} has been modified");
				}

				IoHash hash;
				using (FileStream stream = GetFileInfo(fileState).OpenRead())
				{
					hash = await IoHash.ComputeAsync(stream, cancellationToken);
				}

				if (hash != fileState.Hash)
				{
					throw new InvalidDataException($"Hash for {fileInfo.FullName} was {hash}, expected {fileState.Hash}");
				}
			}
		}

		#endregion

		void AddDirToHashLookup(DirectoryState dirState)
		{
			foreach (DirectoryState subDirState in dirState.Directories)
			{
				AddDirToHashLookup(subDirState);
			}
			foreach (FileState fileState in dirState.Files)
			{
				AddFileToHashLookup(fileState);
			}
		}

		void AddFileToHashLookup(FileState file)
		{
			HashInfo? hashInfo;
			if (!_hashes.TryGetValue(file.Hash, out hashInfo))
			{
				hashInfo = new HashInfo(file.Hash, file.Length);
				_hashes.Add(file.Hash, hashInfo);
			}
			hashInfo.Files.Add(file);
		}
		/*
		void RemoveFileFromHashLookup(FileState file)
		{
			HashInfo? hashInfo;
			if (_hashes.TryGetValue(file.Hash, out hashInfo))
			{
				hashInfo.Files.Remove(file);
			}
		}
		*/
		void MoveFileToCache(FileState fileState, DirectoryState cacheDirState)
		{
			string name = fileState.Hash.ToString();
			for (int idx = 0; idx < 2; idx++)
			{
				cacheDirState = cacheDirState.FindOrAddDirectory(name.Substring(idx * 2, 2));
			}

			FileInfo fileInfo = fileState.GetFileReference(_rootDir).ToFileInfo();
			if (fileInfo.Exists)
			{
				if (cacheDirState.ContainsFile(name) || fileState.IsModified(fileInfo))
				{
					DeleteFile(fileState);
				}
				else
				{
					MoveFile(fileState, cacheDirState, name);
					fileState.LayerFlags = CacheLayerFlag;
				}
			}
		}

		void MoveFile(FileState fileState, DirectoryState targetDirState, string targetName)
		{
			FileReference sourceFile = fileState.GetFileReference(_rootDir);
			fileState.MoveTo(targetDirState, targetName);

			FileReference targetFile = fileState.GetFileReference(_rootDir);
			DirectoryReference.CreateDirectory(targetFile.Directory);
			FileReference.Move(sourceFile, targetFile, true);

			_logger.LogDebug("Moving file from {Source} to {Target}", sourceFile, targetFile);
		}

		void DeleteFile(FileState fileState)
		{
			FileReference fileRef = fileState.GetFileReference(_rootDir);
			FileUtils.ForceDeleteFile(fileRef);
			fileState.Delete();

			_logger.LogDebug("Deleting file {File}", fileRef);
		}

		FileInfo GetFileInfo(FileState file)
		{
			return file.GetFileReference(_rootDir).ToFileInfo();
		}
	}
}
