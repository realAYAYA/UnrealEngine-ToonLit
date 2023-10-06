// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Security.Cryptography;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Nodes;
using EpicGames.Perforce;
using EpicGames.Perforce.Managed;
using Horde.Server.Storage;
using Horde.Server.Streams;
using Horde.Server.Utilities;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Logging;

namespace Horde.Server.Perforce
{
	/// <summary>
	/// Options for replicating commits
	/// </summary>
	class PerforceReplicationOptions
	{
		public bool IncludeContent { get; set; }
		public BundleOptions TreeOptions { get; set; } = new BundleOptions();
		public ChunkingOptions ChunkingOptions { get; set; } = new ChunkingOptions();
		public RefOptions RefOptions { get; set; } = new RefOptions();
	}

	/// <summary>
	/// Replicates commits from Perforce into Horde's internal storage
	/// </summary>
	class PerforceReplicator
	{
		[NodeType("{8C874966-2E89-4273-A4AB-AC9F5491C86D}")]
		class SyncNode : Node
		{
			public int Change { get; }
			public int ParentChange { get; }
			public NodeRef<DirectoryNode> Contents { get; set; }
			public List<Utf8String> Paths { get; }

			public SyncNode(int number, int parentNumber, NodeRef<DirectoryNode> contents)
			{
				Change = number;
				ParentChange = parentNumber;
				Contents = contents;
				Paths = new List<Utf8String>();
			}

			public SyncNode(NodeReader reader)
			{
				Change = (int)reader.ReadUnsignedVarInt();
				ParentChange = (int)reader.ReadUnsignedVarInt();
				Contents = reader.ReadNodeRef<DirectoryNode>();
				Paths = reader.ReadList(() => reader.ReadUtf8String());
			}

			/// <inheritdoc/>
			public override void Serialize(NodeWriter writer)
			{
				writer.WriteUnsignedVarInt(Change);
				writer.WriteUnsignedVarInt(ParentChange);
				writer.WriteNodeRef(Contents);
				writer.WriteList(Paths, x => writer.WriteUtf8String(x));
			}
		}

		// Partial mirror of FileSysType from P4 API (filesys.h)
//		const uint FST_TEXT = 0x0001;
		const uint FST_BINARY = 0x0002;
//		const uint FST_UNICODE = 0x000c;
		const uint FST_UTF16 = 0x000e;
//		const uint FST_UTF8 = 0x000f;
		const uint FST_MASK = 0x000f;

		// Mirrors FilePerm from P4 API (filesys.h)
		const uint FPM_RO = 0;     // leave file read-only
//		const uint FPM_RW = 1;     // leave file read-write
		const uint FPM_ROO = 2;    // leave file read-only (owner)
		const uint FPM_RXO = 3;    // set file read-execute (owner) NO W
//		const uint FPM_RWO = 4;    // set file read-write (owner) NO X
		const uint FPM_RWXO = 5;   // set file read-write-execute (owner)

		[DebuggerDisplay("{_path}")]
		class DirectoryToSync
		{
			public readonly Utf8String Path;
			public readonly Dictionary<Utf8String, long> FileNameToSize;
			public long _size;

			public DirectoryToSync(Utf8String path, Utf8StringComparer comparer)
			{
				Path = path;
				FileNameToSize = new Dictionary<Utf8String, long>(comparer);
			}
		}

		record class FileInfo(Utf8String Path, FileEntryFlags Flags, long Length, byte[] Md5, NodeRef<ChunkedDataNode> NodeRef);

		class FileWriter : IDisposable
		{
			class Handle
			{
				public Utf8String _path;
				public FileEntryFlags _flags;
				public readonly ChunkedDataWriter _fileWriter;
				public long _size;
				public long _sizeWritten;
				public readonly IncrementalHash _hash = IncrementalHash.CreateHash(HashAlgorithmName.MD5);

				public Handle(IStorageWriter writer, ChunkingOptions options)
				{
					_fileWriter = new ChunkedDataWriter(writer, options);
				}
			}

			readonly IStorageWriter _writer;
			readonly ChunkingOptions _options;
			readonly Stack<Handle> _freeHandles = new Stack<Handle>();
			readonly Dictionary<int, Handle> _openHandles = new Dictionary<int, Handle>();

			public FileWriter(IStorageWriter writer, ChunkingOptions options)
			{
				_writer = writer;
				_options = options;
			}

			public void Dispose()
			{
				foreach (Handle handle in _freeHandles)
				{
					handle._hash.Dispose();
				}
				foreach (Handle handle in _openHandles.Values)
				{
					handle._hash.Dispose();
				}
			}

			public void Open(int fd, Utf8String path, long size, FileEntryFlags flags)
			{
				Handle? handle;
				if (!_freeHandles.TryPop(out handle))
				{
					handle = new Handle(_writer, _options);
				}

				handle._fileWriter.Reset();
				handle._path = path;
				handle._size = size;
				handle._sizeWritten = 0;
				handle._flags = flags;

				_openHandles.Add(fd, handle);
			}

			public async Task AppendAsync(int fd, ReadOnlyMemory<byte> data, CancellationToken cancellationToken)
			{
				Handle handle = _openHandles[fd];
				handle._hash.AppendData(data.Span);

				await handle._fileWriter.AppendAsync(data, cancellationToken);
				handle._sizeWritten += data.Length;
			}

			public async Task<FileInfo> CloseAsync(int fd, CancellationToken cancellationToken)
			{
				Handle handle = _openHandles[fd];
				if (handle._sizeWritten != handle._size)
				{
					throw new ReplicationException($"Invalid size for replicated file '{handle._path}'. Expected {handle._size}, got {handle._sizeWritten}.");
				}

				NodeRef<ChunkedDataNode> nodeRef = await handle._fileWriter.FlushAsync(cancellationToken);
				byte[] hash = handle._hash.GetHashAndReset();
				FileInfo info = new FileInfo(handle._path, handle._flags, handle._size, hash, nodeRef);

				_openHandles.Remove(fd);
				_freeHandles.Push(handle);

				return info;
			}
		}

		class ReplicationClient
		{
			public PerforceSettings Settings { get; }
			public string ClusterName { get; }
			public InfoRecord ServerInfo { get; }
			public ClientRecord Client { get; }
			public int Change { get; set; }

			public ReplicationClient(PerforceSettings settings, string clusterName, InfoRecord serverInfo, ClientRecord client, int change)
			{
				Settings = settings;
				ClusterName = clusterName;
				ServerInfo = serverInfo;
				Client = client;
				Change = change;
			}
		}

		readonly Dictionary<StreamId, ReplicationClient> _cachedPerforceClients = new Dictionary<StreamId, ReplicationClient>();

		readonly IPerforceService _perforceService;
		readonly StorageService _storageService;
		readonly IMemoryCache _memoryCache;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public PerforceReplicator(IPerforceService perforceService, StorageService storageService, IMemoryCache memoryCache, ILogger<PerforceReplicator> logger)
		{
			Node.RegisterType<SyncNode>();

			_perforceService = perforceService;
			_storageService = storageService;
			_memoryCache = memoryCache;
			_logger = logger;
		}

		/// <summary>
		/// Gets the ref name for a given stream
		/// </summary>
		/// <param name="streamId">The stream to get a ref for</param>
		/// <returns>Ref name for the stream</returns>
		public static RefName GetRefName(StreamId streamId) => new RefName(streamId.ToString());

		/// <summary>
		/// Gets the ref name for a given stream
		/// </summary>
		/// <param name="streamId">The stream to get a ref for</param>
		/// <returns>Ref name for the stream</returns>
		static RefName GetIncrementalRefName(StreamId streamId) => new RefName($"{streamId}/incremental");

		/// <summary>
		/// Replicates a change to storage
		/// </summary>
		/// <param name="streamConfig">Stream to replicate data from</param>
		/// <param name="change">Changelist to replicate</param>
		/// <param name="options">Options for replication</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public async Task WriteAsync(StreamConfig streamConfig, int change, PerforceReplicationOptions options, CancellationToken cancellationToken)
		{
			IStorageClient store = await _storageService.GetClientAsync(Namespace.Perforce, cancellationToken);

			// Find the parent node
			RefName refName = GetRefName(streamConfig.Id);

			CommitNode? parent = null;
			NodeRef<CommitNode>? parentRef = await store.TryReadRefTargetAsync<CommitNode>(refName, cancellationToken: cancellationToken);
			while (parentRef != null)
			{
				parent = await parentRef.ExpandAsync(cancellationToken);
				if (parent.Number < change)
				{
					break;
				}
				parentRef = parent.Parent;
			}

			int parentChange = parent?.Number ?? 0;

			// Read the current incremental state or create a new node to track the incremental state
			RefName incRefName = GetIncrementalRefName(streamConfig.Id);
			SyncNode? syncNode = await store.TryReadNodeAsync<SyncNode>(incRefName, cancellationToken: cancellationToken);
			if (syncNode == null || syncNode.Change != change || syncNode.ParentChange != parentChange)
			{
				if (parent == null)
				{
					syncNode = new SyncNode(change, parentChange, null!);
				}
				else
				{
					syncNode = new SyncNode(change, parentChange, parent.Contents);
				}
			}

			// Get the root node
			DirectoryNode root;
			if (syncNode.Contents != null)
			{
				root = await syncNode.Contents.ExpandAsync(cancellationToken);
			}
			else
			{
				root = new DirectoryNode();
			}

			// Create a client to replicate from this stream
			ReplicationClient clientInfo = await FindOrAddReplicationClientAsync(streamConfig);

			// Connect to the server and flush the workspace
			using IPerforceConnection perforce = await PerforceConnection.CreateAsync(clientInfo.Settings, _logger);

			// Apply all the updates
			_logger.LogInformation("Syncing client {Client} from changelist {BaseChange} to {Change}", clientInfo.Client.Name, parentChange, change);
			await FlushWorkspaceAsync(clientInfo, perforce, parentChange);
			clientInfo.Change = -1;

			Utf8String clientRoot = new Utf8String(clientInfo.Client.Root);
			string queryPath = $"//{clientInfo.Client.Name}/...";

			// Replay the files that have already been synced
			foreach (Utf8String path in syncNode.Paths)
			{
				string flushPath = $"//{clientInfo.Client.Name}/{path}@{change}";
				_logger.LogInformation("Flushing {FlushPath}", flushPath);
				await perforce.SyncQuietAsync(SyncOptions.Force | SyncOptions.KeepWorkspaceFiles, -1, flushPath, cancellationToken);
			}

			// Add the root directory from the filter to the list of files to sync. This prevents traversing above it.
			Dictionary<Utf8String, DirectoryToSync> pathToDirectory = new Dictionary<Utf8String, DirectoryToSync>(clientInfo.ServerInfo.Utf8PathComparer);

			// Do a sync preview to find everything that's left, and sort the remaining list of paths
			await foreach (PerforceResponse<SyncRecord> response in perforce.StreamCommandAsync<SyncRecord>("sync", new[] { "-n" }, new string[] { $"{queryPath}@{change}" }, null, cancellationToken))
			{
				PerforceError? error = response.Error;
				if (error != null)
				{
					_logger.LogWarning("Perforce: {Message}", error.Data);
					continue;
				}

				Utf8String path = response.Data.Path;
				if (!path.StartsWith(clientRoot, Utf8StringComparer.Ordinal))
				{
					throw new ArgumentException($"Unable to make path {path} relative to client root {clientRoot}");
				}

				int fileIdx = path.Length;
				while (fileIdx > 0 && path[fileIdx - 1] != '/' && path[fileIdx - 1] != '\\')
				{
					fileIdx--;
				}

				Utf8String directoryPath = path.Slice(clientRoot.Length, fileIdx - clientRoot.Length);

				DirectoryToSync directory = FindOrAddDirectoryTree(pathToDirectory, directoryPath, clientInfo.ServerInfo.Utf8PathComparer);
				directory.FileNameToSize.Add(path.Slice(fileIdx).Clone(), response.Data.FileSize);
				directory._size += response.Data.FileSize;
			}

			// Sort the directories by name to ensure that they are consistent between runs
			List<DirectoryToSync> directories = pathToDirectory.Values.OrderBy(x => x.Path, clientInfo.ServerInfo.Utf8PathComparer).ToList();

			// Output some stats for the sync
			long totalSize = directories.Sum(x => x._size);
			_logger.LogInformation("Total sync size: {Size:n1}mb", totalSize / (1024.0 * 1024.0));

			// Create the tree writer
			await using IStorageWriter writer = store.CreateWriter(refName);

			// Keep track of changes to make to the directory structure
			DirectoryUpdate rootUpdate = new DirectoryUpdate();

			// Sync incrementally
			long syncedSize = 0;
			while (directories.Count > 0)
			{
				// Save the incremental state
				if (syncedSize > 0)
				{
					Stopwatch flushTimer = Stopwatch.StartNew();

					await root.UpdateAsync(rootUpdate, writer, cancellationToken);
					syncNode.Contents = await writer.WriteNodeAsync(root, cancellationToken);
					NodeRef<SyncNode> syncNodeRef = await writer.WriteNodeAsync(syncNode, cancellationToken);
					await store.WriteRefTargetAsync(incRefName, syncNodeRef, cancellationToken: cancellationToken);
					rootUpdate.Clear();

					flushTimer.Stop();
				}

				// Find the next paths to sync
				const long MaxBatchSize = 1L * 1024 * 1024 * 1024;

				int dirIdx = directories.Count - 1;
				long size = directories[dirIdx]._size;

				for (; dirIdx > 0; dirIdx--)
				{
					long nextSize = size + directories[dirIdx - 1]._size;
					if (size > 0 && nextSize > MaxBatchSize)
					{
						break;
					}
					size = nextSize;
				}

				syncedSize += size;
				double syncPct = (totalSize == 0) ? 100.0 : (syncedSize * 100.0) / totalSize;
				_logger.LogInformation("Syncing {StreamId} to {Change} [{SyncPct:n1}%] ({Size:n1}mb)", streamConfig.Id, change, syncPct, size / (1024.0 * 1024.0));

				// Copy them to a separate list and remove any redundant paths
				List<string> syncPaths = new List<string>();
				for (int idx = dirIdx; idx < directories.Count; idx++)
				{
					Utf8String basePath = directories[idx].Path;
					syncPaths.Add($"//{clientInfo.Client.Name}/{basePath}...@{change}");

					long dirSize = directories[idx]._size;
					while (idx + 1 < directories.Count && directories[idx + 1].Path.StartsWith(basePath, clientInfo.ServerInfo.Utf8PathComparer))
					{
						dirSize += directories[idx + 1]._size;
						idx++;
					}

					_logger.LogInformation("  {Directory} ({Size:n1}mb)", directories[idx].Path + "...", dirSize / (1024.0 * 1024.0));
				}

				Stopwatch syncTimer = Stopwatch.StartNew();
				Stopwatch processTimer = new Stopwatch();
				Stopwatch gcTimer = new Stopwatch();

				using FileWriter fileWriter = new FileWriter(writer, options.ChunkingOptions);
				await foreach (PerforceResponse response in perforce.StreamCommandAsync("sync", Array.Empty<string>(), syncPaths, null, typeof(SyncRecord), true, default))
				{
					PerforceError? error = response.Error;
					if (error != null)
					{
						if (error.Generic == PerforceGenericCode.Empty)
						{
							continue;
						}
						else
						{
							throw new ReplicationException($"Perforce error while replicating content - {error}");
						}
					}

					processTimer.Start();

					PerforceIo? io = response.Io;
					if (io != null)
					{
						if (io.Command == PerforceIoCommand.Open)
						{
							UnpackOpenPayload(io.Payload, out Utf8String path, out int type, out int mode, out int perms);

							FileEntryFlags flags = 0;
							if ((type & FST_MASK) != FST_BINARY)
							{
								flags |= FileEntryFlags.Text;
							}
							if ((type & FST_UTF16) != 0)
							{
								flags |= FileEntryFlags.Utf16;
							}
							if (perms == FPM_RO || perms == FPM_ROO || perms == FPM_RXO)
							{
								flags |= FileEntryFlags.ReadOnly;
							}
							if (perms == FPM_RWXO || perms == FPM_RXO)
							{
								flags |= FileEntryFlags.Executable;
							}

							Utf8String file = GetClientRelativePath(path, clientInfo.Client.Root);
							int offset = GetFileOffset(file);

							long fileSize = 0;
							if (!pathToDirectory.TryGetValue(file.Substring(0, offset), out DirectoryToSync? directory))
							{
								throw new ReplicationException($"Unable to find directory for {file}");
							}
							if (!directory.FileNameToSize.TryGetValue(file.Substring(offset), out fileSize))
							{
								throw new ReplicationException($"Unable to find file entry for {file}");
							}

							fileWriter.Open(io.File, file, fileSize, flags);
						}
						else if (io.Command == PerforceIoCommand.Write)
						{
							await fileWriter.AppendAsync(io.File, io.Payload, cancellationToken);
						}
						else if (io.Command == PerforceIoCommand.Close)
						{
							FileInfo info = await fileWriter.CloseAsync(io.File, cancellationToken);
							FileEntry entry = rootUpdate.AddFile(info.Path.ToString(), info.Flags, info.Length, info.NodeRef);
							entry.CustomData = info.Md5;
						}
						else if (io.Command == PerforceIoCommand.Unlink)
						{
							UnpackUnlinkPayload(io.Payload, out Utf8String path);
							Utf8String file = GetClientRelativePath(path, clientInfo.Client.Root);
							await root.DeleteFileByPathAsync(file, cancellationToken);
						}
						else
						{
							_logger.LogWarning("Unhandled command code {Code}", io.Command);
						}
					}

					processTimer.Stop();
				}

				TimeSpan stallTime = (perforce as NativePerforceConnection)?.StallTime ?? TimeSpan.Zero;
				_logger.LogInformation("Completed batch in {TimeSeconds:n1}s ({ProcessTimeSeconds:n1}s processing, {StallTimeSeconds:n1}s stalled, {Throughput:n1}mb/s)", syncTimer.Elapsed.TotalSeconds, processTimer.Elapsed.TotalSeconds, stallTime.TotalSeconds, size / (1024.0 * 1024.0 * syncTimer.Elapsed.TotalSeconds));

				// Combine all the existing sync paths together with a new wildcard.
				while (directories.Count > dirIdx)
				{
					Utf8String nextPath = directories[^1].Path;
					if (syncNode.Paths.Count > 0)
					{
						Utf8String lastPath = syncNode.Paths[^1];
						for (int endIdx = 0; endIdx < lastPath.Length; endIdx++)
						{
							if (lastPath[endIdx] == '/')
							{
								Utf8String prefix = lastPath.Substring(0, endIdx + 1);
								if (!nextPath.StartsWith(prefix, clientInfo.ServerInfo.Utf8PathComparer))
								{
									// Remove any paths that start with this prefix
									while (syncNode.Paths.Count > 0 && syncNode.Paths[^1].StartsWith(prefix, clientInfo.ServerInfo.Utf8PathComparer))
									{
										syncNode.Paths.RemoveAt(syncNode.Paths.Count - 1);
									}

									// Replace it with a wildcard
									syncNode.Paths.Add(prefix + "...");
									break;
								}
							}
						}
					}
					syncNode.Paths.Add(nextPath + "...");
					directories.RemoveAt(directories.Count - 1);
				}
			}

			// Create the commit node
			ChangeRecord changeRecord = await perforce.GetChangeAsync(GetChangeOptions.None, change, cancellationToken);
			DirectoryNodeRef rootRef = new DirectoryNodeRef(root.Length, await writer.WriteNodeAsync(root, cancellationToken));
			CommitNode commitNode = new CommitNode(change, parentRef, changeRecord.User ?? "Unknown", changeRecord.Description ?? String.Empty, changeRecord.Date, rootRef);
			await store.WriteNodeAsync(refName, commitNode, refOptions: options.RefOptions, cancellationToken: cancellationToken); 
		}

		static int GetFileOffset(Utf8String path)
		{
			int fileIdx = path.Length;
			while (fileIdx > 0 && path[fileIdx - 1] != '/' && path[fileIdx - 1] != '\\')
			{
				fileIdx--;
			}
			return fileIdx;
		}

		static DirectoryToSync FindOrAddDirectoryTree(Dictionary<Utf8String, DirectoryToSync> pathToDirectory, Utf8String directoryPath, Utf8StringComparer comparer)
		{
			DirectoryToSync? directory;
			if (!pathToDirectory.TryGetValue(directoryPath, out directory))
			{
				// Add a new path
				Utf8String normalizedPath = NormalizePathSeparators(directoryPath);
				directory = new DirectoryToSync(normalizedPath, comparer);
				pathToDirectory.Add(directoryPath, directory);

				// Also add all the parent directories. This makes the logic for combining wildcards simpler.
				for (int idx = normalizedPath.Length - 2; idx > 0; idx--)
				{
					if (normalizedPath[idx] == '/')
					{
						Utf8String parentDirectoryPath = directoryPath.Substring(0, idx + 1);
						if (pathToDirectory.ContainsKey(parentDirectoryPath))
						{
							break;
						}
						else
						{
							pathToDirectory.Add(parentDirectoryPath, new DirectoryToSync(normalizedPath.Substring(0, idx + 1), comparer));
						}
					}
				}
			}
			return directory;
		}

		static Utf8String NormalizePathSeparators(Utf8String path)
		{
			byte[] newPath = new byte[path.Length];
			for (int idx = 0; idx < path.Length; idx++)
			{
				newPath[idx] = (path[idx] == '\\') ? (byte)'/' : path[idx];
			}
			return new Utf8String(newPath);
		}

		async Task FlushWorkspaceAsync(ReplicationClient clientInfo, IPerforceConnection perforce, int change)
		{
			if (clientInfo.Change != change)
			{
				clientInfo.Change = -1;
				if (change == 0)
				{
					_logger.LogInformation("Flushing have table for {Client}", clientInfo.Client.Name);
					await perforce.SyncQuietAsync(SyncOptions.Force | SyncOptions.KeepWorkspaceFiles, -1, $"//{clientInfo.Client.Name}/...#0");
				}
				else
				{
					_logger.LogInformation("Flushing have table for {Client} to change {Change}", clientInfo.Client.Name, change);
					await perforce.SyncQuietAsync(SyncOptions.Force | SyncOptions.KeepWorkspaceFiles, -1, $"//{clientInfo.Client.Name}/...@{change}");
				}
				clientInfo.Change = change;
			}
		}

		static void UnpackOpenPayload(ReadOnlyMemory<byte> data, out Utf8String path, out int flags, out int type, out int perms)
		{
			int length = data.Span.IndexOf((byte)0);
			if (length == -1)
			{
				throw new InvalidStreamException("Invalid data returned by Perforce server; open path does not contain null terminator.");
			}

			path = new Utf8String(data.Slice(0, length)).Clone();
			flags = BinaryPrimitives.ReadInt32LittleEndian(data.Span.Slice(length + 1, sizeof(int)));
			type = BinaryPrimitives.ReadInt32LittleEndian(data.Span.Slice(length + 5, sizeof(int)));
			perms = BinaryPrimitives.ReadInt32LittleEndian(data.Span.Slice(length + 9, sizeof(int)));
		}

		static void UnpackUnlinkPayload(ReadOnlyMemory<byte> data, out Utf8String path)
		{
			int length = data.Span.IndexOf((byte)0);
			if (length != -1)
			{
				data = data.Slice(0, length);
			}
			path = new Utf8String(data).Clone();
		}

		static Utf8String GetClientRelativePath(Utf8String path, Utf8String clientRoot)
		{
			if (!path.StartsWith(clientRoot, Utf8StringComparer.Ordinal))
			{
				throw new ArgumentException($"Unable to make path {path} relative to client root {clientRoot}");
			}
			return path.Substring(clientRoot.Length);
		}

		async Task<ReplicationClient?> FindReplicationClientAsync(StreamConfig streamConfig)
		{
			ReplicationClient? clientInfo;
			if (_cachedPerforceClients.TryGetValue(streamConfig.Id, out clientInfo))
			{
				if (!String.Equals(clientInfo.ClusterName, streamConfig.ClusterName, StringComparison.Ordinal) && String.Equals(clientInfo.Client.Stream, streamConfig.Name, StringComparison.Ordinal))
				{
					PerforceSettings serverSettings = new PerforceSettings(clientInfo.Settings);
					serverSettings.ClientName = null;

					using IPerforceConnection perforce = await PerforceConnection.CreateAsync(_logger);
					await perforce.DeleteClientAsync(DeleteClientOptions.None, clientInfo.Client.Name);

					_cachedPerforceClients.Remove(streamConfig.Id);
					clientInfo = null;
				}
			}
			return clientInfo;
		}

		async Task<ReplicationClient> FindOrAddReplicationClientAsync(StreamConfig streamConfig)
		{
			ReplicationClient? clientInfo = await FindReplicationClientAsync(streamConfig);
			if (clientInfo == null)
			{
				using IPerforceConnection? perforce = await _perforceService.ConnectAsync(streamConfig.ClusterName);
				if (perforce == null)
				{
					throw new PerforceException($"Unable to create connection to Perforce server");
				}

				InfoRecord serverInfo = await perforce.GetInfoAsync(InfoOptions.ShortOutput);

				ClientRecord newClient = new ClientRecord($"Horde.Build_Rep_{serverInfo.ClientHost}_{streamConfig.Id}", perforce.Settings.UserName, "/p4/");
				newClient.Description = "Created to mirror Perforce content to Horde Storage";
				newClient.Owner = perforce.Settings.UserName;
				newClient.Host = serverInfo.ClientHost;
				newClient.Stream = streamConfig.ReplicationStream ?? streamConfig.Name;
				newClient.Type = "readonly";
				await perforce.CreateClientAsync(newClient);
				_logger.LogInformation("Created client {ClientName} for {StreamName}", newClient.Name, streamConfig.Name);

				PerforceSettings settings = new PerforceSettings(perforce.Settings);
				settings.ClientName = newClient.Name;
				settings.PreferNativeClient = true;

				clientInfo = new ReplicationClient(settings, streamConfig.ClusterName, serverInfo, newClient, -1);
				_cachedPerforceClients.Add(streamConfig.Id, clientInfo);
			}
			return clientInfo;
		}
	}
}