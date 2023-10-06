// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Backends;
using EpicGames.Horde.Storage.Nodes;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Logging.Abstractions;

namespace UnrealBuildTool.Artifacts
{
	/// <summary>
	/// Horde specific artifact action structure that also contains the file nodes for the outputs
	/// </summary>
	readonly struct HordeArtifactAction
	{

		/// <summary>
		/// Artifact action
		/// </summary>
		public readonly ArtifactAction ArtifactAction;

		/// <summary>
		/// Collection of output file references.  There should be exactly the same number
		/// of file references as outputs in the action
		/// </summary>
		public readonly NodeRef<ChunkedDataNode>[] OutputRefs;

		/// <summary>
		/// Construct a new horde artifact number
		/// </summary>
		/// <param name="artifactAction">Artifact action</param>
		/// <exception cref="ArgumentException"></exception>
		public HordeArtifactAction(ArtifactAction artifactAction)
		{
			ArtifactAction = artifactAction;
			OutputRefs = new NodeRef<ChunkedDataNode>[ArtifactAction.Outputs.Length];
		}

		/// <summary>
		/// Construct a new artifact action from the reader
		/// </summary>
		/// <param name="reader">Source reader</param>
		public HordeArtifactAction(NodeReader reader)
		{
			ArtifactAction = reader.ReadArtifactAction();
			OutputRefs = reader.ReadVariableLengthArray(() => new NodeRef<ChunkedDataNode>(reader));
		}

		/// <summary>
		/// Serialize the artifact action 
		/// </summary>
		/// <param name="writer">Destination writer</param>
		public void Serialize(NodeWriter writer)
		{
			writer.WriteArtifactAction(ArtifactAction);
			writer.WriteVariableLengthArray(OutputRefs, x => x.Serialize(writer));
		}

		/// <summary>
		/// Return an enumeration of the references
		/// </summary>
		/// <returns></returns>
		public IEnumerable<NodeRef> EnumerateRefs() => OutputRefs;

		/// <summary>
		/// Write all the files to disk
		/// </summary>
		/// <param name="writer">Destination writer</param>
		/// <param name="cancellationToken">Cancellation token</param>
		/// <returns>Task</returns>
		public async Task WriteFilesAsync(IStorageWriter writer, CancellationToken cancellationToken)
		{
			LeafChunkedDataNodeOptions leafOptions = new(512 * 1024, 1 * 1024 * 1024, 2 * 1024 * 1024);
			InteriorChunkedDataNodeOptions interiorOptions = new(1, 10, 20);
			ChunkingOptions options = new() { LeafOptions = leafOptions, InteriorOptions = interiorOptions };

			ChunkedDataWriter fileWriter = new(writer, options);
			int index = 0;
			foreach (ArtifactFile artifact in ArtifactAction.Outputs)
			{
				string outputName = artifact.GetFullPath(ArtifactAction.DirectoryMapping);
				using FileStream stream = new(outputName, FileMode.Open, FileAccess.Read, FileShare.Read);
				OutputRefs[index++] = new NodeRef<ChunkedDataNode>(await fileWriter.CreateAsync(stream, leafOptions.TargetSize, cancellationToken));
			}
		}
	}

	/// <summary>
	/// Series of helper methods for serialization
	/// </summary>
	static class HordeArtifactReaderWriterExtensions
	{

		/// <summary>
		/// Read a horde artifact action
		/// </summary>
		/// <param name="reader">Source reader</param>
		/// <returns>Created artifact action</returns>
		public static HordeArtifactAction ReadHordeArtifactAction(this NodeReader reader)
		{
			return new HordeArtifactAction(reader);
		}

		/// <summary>
		/// Write a horde artifact action
		/// </summary>
		/// <param name="writer">Destination writer</param>
		/// <param name="artifactAction">Artifact action to write</param>
		public static void WriteHordeArtifactAction(this NodeWriter writer, HordeArtifactAction artifactAction)
		{
			artifactAction.Serialize(writer);
		}
	}

	/// <summary>
	/// Horde node that represents a collection of action nodes 
	/// </summary>
	[NodeType("{E8DBCD77-861D-4CAE-B77F-5807D26E2533}")]
	class ArtifactActionCollectionNode : Node
	{

		/// <summary>
		/// Collection of actions
		/// </summary>
		public Dictionary<IoHash, HordeArtifactAction> ArtifactActions = new();

		/// <summary>
		/// Construct a new collection
		/// </summary>
		public ArtifactActionCollectionNode()
		{
		}

		/// <summary>
		/// Construct a new artifact action collection from the source reader
		/// </summary>
		/// <param name="reader">Source reader</param>
		public ArtifactActionCollectionNode(NodeReader reader)
		{
			ArtifactActions = reader.ReadDictionary<IoHash, HordeArtifactAction>(() => reader.ReadIoHash(), () => reader.ReadHordeArtifactAction());
		}

		/// <inheritdoc/>
		public override void Serialize(NodeWriter writer)
		{
			writer.WriteDictionary<IoHash, HordeArtifactAction>(ArtifactActions, (x) => writer.WriteIoHash(x), (x) => writer.WriteHordeArtifactAction(x));
		}

		/// <summary>
		/// Mark the node as dirty
		/// </summary>
		public new void MarkAsDirty()
		{
			base.MarkAsDirty();
		}
	}

	/// <summary>
	/// Class for managing artifacts using horde storage
	/// </summary>
	public class HordeStorageArtifactCache : IArtifactCache
	{
		/// <summary>
		/// Defines the theoretical max number of pending actions to write
		/// </summary>
		const int MaxPendingSize = 128;

		/// <summary>
		/// Underlying storage object
		/// </summary>
		private IStorageClient? _store = null;

		/// <summary>
		/// Logger to be used
		/// </summary>
		private readonly ILogger _logger;

		/// <summary>
		/// Task used to wait on ready state
		/// </summary>
		private Task<ArtifactCacheState>? _readyTask = null;

		/// <summary>
		/// Ready state
		/// </summary>
		private int _state = (int)ArtifactCacheState.Pending;

		/// <summary>
		/// Collection of actions waiting to be written
		/// </summary>
		private readonly List<ArtifactAction> _pendingWrites;

		/// <summary>
		/// Task for any pending flush
		/// </summary>
		private Task? _pendingWritesFlushTask = null;

		/// <summary>
		/// Controls access to shared data structures
		/// </summary>
		private readonly SemaphoreSlim _semaphore = new(1);

		/// <summary>
		/// Test to see if the cache is ready
		/// </summary>
		public ArtifactCacheState State
		{
			get => (ArtifactCacheState)Interlocked.Add(ref _state, 0);
			private set => Interlocked.Exchange(ref _state, (int)value);
		}

		/// <summary>
		/// Create a memory only cache
		/// </summary>
		/// <returns>Storage client instance</returns>
		public static IArtifactCache CreateMemoryCache(ILogger logger)
		{
			HordeStorageArtifactCache cache = new(new MemoryStorageClient(), logger)
			{
				State = ArtifactCacheState.Available
			};
			return cache;
		}

		/// <summary>
		/// Create a file based cache
		/// </summary>
		/// <param name="directory">Destination directory</param>
		/// <param name="logger">Logging object</param>
		/// <param name="cleanDirectory">If true, clean the directory</param>
		/// <returns>Storage client instance</returns>
		public static IArtifactCache CreateFileCache(DirectoryReference directory, ILogger logger, bool cleanDirectory)
		{
			HordeStorageArtifactCache cache = new(null, logger);
			cache._readyTask = Task.Run(() => cache.InitFileCache(directory, NullLogger.Instance, cleanDirectory));
			return cache;
		}

		static HordeStorageArtifactCache()
		{
			Node.RegisterType<ArtifactActionCollectionNode>();
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="storage">Storage object to use</param>
		/// <param name="logger">Logging destination</param>
		private HordeStorageArtifactCache(IStorageClient? storage, ILogger logger)
		{
			_store = storage;
			_logger = logger;
			_pendingWrites = new(MaxPendingSize);
		}

		/// <inheritdoc/>
		public Task<ArtifactCacheState> WaitForReadyAsync()
		{
			return _readyTask ?? Task.FromResult<ArtifactCacheState>(State);
		}

		/// <inheritdoc/>
		public async Task<ArtifactAction[]> QueryArtifactActionsAsync(IoHash[] partialKeys, CancellationToken cancellationToken)
		{
			if (State != ArtifactCacheState.Available || _store == null)
			{
				return Array.Empty<ArtifactAction>();
			}

			List<ArtifactAction> artifactActions = new();
			await _semaphore.WaitAsync(cancellationToken);
			try
			{
				foreach (IoHash key in partialKeys)
				{
					lock (_pendingWrites)
					{
						artifactActions.AddRange(_pendingWrites.Where(x => x.Key == key));
					}
					ArtifactActionCollectionNode? node = await _store.TryReadNodeAsync<ArtifactActionCollectionNode>(GetRefName(key), default, cancellationToken);
					if (node != null)
					{
						foreach (HordeArtifactAction artifactAction in node.ArtifactActions.Values)
						{
							artifactActions.Add(artifactAction.ArtifactAction);
						}
					}
				}
			}
			finally
			{
				_semaphore.Release();
			}
			return artifactActions.ToArray();
		}

		/// <inheritdoc/>
		public async Task<bool[]?> QueryArtifactOutputsAsync(ArtifactAction[] artifactActions, CancellationToken cancellationToken)
		{
			if (State != ArtifactCacheState.Available || _store == null)
			{
				return null;
			}

			bool[] output = new bool[artifactActions.Length];
			Array.Fill(output, false);

			for (int index = 0; index < artifactActions.Length; index++)
			{
				output[index] = false;
				ArtifactAction artifactAction = artifactActions[index];
				ArtifactActionCollectionNode? node = await _store.TryReadNodeAsync<ArtifactActionCollectionNode>(GetRefName(artifactAction.Key), default, cancellationToken);
				if (node != null)
				{
					if (node.ArtifactActions.TryGetValue(artifactAction.ActionKey, out HordeArtifactAction hordeArtifactAction))
					{
						output[index] = true;

						int refIndex = 0;
						foreach (NodeRef<ChunkedDataNode> artifactRef in hordeArtifactAction.OutputRefs)
						{
							if (artifactRef.Handle == null)
							{
								output[index] = false;
								break;
							}
							try
							{
								string outputName = hordeArtifactAction.ArtifactAction.Outputs[refIndex++].GetFullPath(artifactAction.DirectoryMapping);
								using FileStream stream = new(outputName, FileMode.Create, FileAccess.Write, FileShare.ReadWrite);
								await ChunkedDataNode.CopyToStreamAsync(artifactRef.Handle, stream, cancellationToken);
							}
							catch (Exception)
							{
								output[index] = false;
								break;
							}
						}

						if (!output[index])
						{
							foreach (ArtifactFile artifact in hordeArtifactAction.ArtifactAction.Outputs)
							{
								string outputName = artifact.GetFullPath(artifactAction.DirectoryMapping);
								if (File.Exists(outputName))
								{
									try
									{
										File.Delete(outputName);
									}
									catch (Exception)
									{
									}
								}
							}
						}
					}
				}
			}
			return output;
		}

		/// <inheritdoc/>
		public async Task SaveArtifactActionsAsync(ArtifactAction[] artifactActions, CancellationToken cancellationToken)
		{
			if (State != ArtifactCacheState.Available || _store == null)
			{
				return;
			}

			lock (_pendingWrites)
			{
				_pendingWrites.AddRange(artifactActions);
			}

			Task? task = FlushChangesInternalAsync(false, cancellationToken);
			if (task != null)
			{
				await task;
			}
		}

		/// <inheritdoc/>
		public async Task FlushChangesAsync(CancellationToken cancellationToken)
		{
			if (State != ArtifactCacheState.Available || _store == null)
			{
				return;
			}

			Task? task = FlushChangesInternalAsync(true, cancellationToken);
			if (task != null)
			{
				await task;
			}
		}

		/// <summary>
		/// Optionally flush all pending writes
		/// </summary>
		/// <param name="force">If true, force a flush</param>
		/// <param name="cancellationToken">Cancellation token</param>
		private Task? FlushChangesInternalAsync(bool force, CancellationToken cancellationToken)
		{
			Task? pendingFlushTask = null;
			lock (_pendingWrites)
			{

				// If any prior flush task has completed, then forget it
				if (_pendingWritesFlushTask != null && _pendingWritesFlushTask.IsCompleted)
				{
					_pendingWritesFlushTask = null;
				}

				// We start a new flush under the following condition
				//
				// 1) Actions must be pending
				// 2) Create a new task if force is specified
				// 3) -OR- Create a new task if there is no current task and we have reached the limit 
				if (_pendingWrites.Count > 0 && (force || (_pendingWrites.Count >= MaxPendingSize && _pendingWritesFlushTask == null)))
				{
					ArtifactAction[] artifactActionsToFlush = _pendingWrites.ToArray();
					Task? priorTask = _pendingWritesFlushTask;
					_pendingWrites.Clear();
					async Task action()
					{

						// When forcing, we might have a prior flush task in progress.  Wait for it to complete
						if (priorTask != null)
						{
							await priorTask;
						}

						// Block reading while we update the actions
						await _semaphore.WaitAsync(cancellationToken);
						try
						{
							List<Task> tasks = CommitArtifactActions(artifactActionsToFlush, cancellationToken);
							await Task.WhenAll(tasks);
						}
						finally
						{
							_semaphore.Release();
						}
					}
					pendingFlushTask = _pendingWritesFlushTask = new(() => action().Wait(), cancellationToken);
				}
			}

			// Start the task outside of the lock
			pendingFlushTask?.Start();
			return pendingFlushTask;
		}

		/// <summary>
		/// Add a group of artifact actions to a new or existing source
		/// </summary>
		/// <param name="artifactActions">New artifact actions to add</param>
		/// <param name="cancellationToken">Token to be used to cancel operations</param>
		/// <returns>List of tasks</returns>
		private List<Task> CommitArtifactActions(ArtifactAction[] artifactActions, CancellationToken cancellationToken)
		{
			List<Task> tasks = new();
			if (artifactActions.Length == 0)
			{
				return tasks;
			}

			// Loop through the artifact actions
			foreach (ArtifactAction artifactAction in artifactActions)
			{

				// Create the task to write the files
				tasks.Add(Task.Run(async () =>
				{
					RefName refName = GetRefName(artifactAction.Key);

					// Locate the destination collection for this key
					ArtifactActionCollectionNode? node = _store!.TryReadNodeAsync<ArtifactActionCollectionNode>(refName, default, cancellationToken).Result;
					node ??= new ArtifactActionCollectionNode();

					// Update the artifact action collection
					HordeArtifactAction hordeArtifactAction = new(artifactAction);
					node.ArtifactActions[artifactAction.ActionKey] = hordeArtifactAction;
					node.MarkAsDirty();

					// Save the artifact action file
					await using IStorageWriter writer = _store!.CreateWriter();
					await hordeArtifactAction.WriteFilesAsync(writer, cancellationToken);

					// Save the collection
					BlobHandle _ = await _store.WriteNodeAsync(refName, node, cancellationToken: cancellationToken);
				}, cancellationToken));
			}
			return tasks;
		}

		/// <summary>
		/// Initialize a file based cache
		/// </summary>
		/// <param name="directory">Destination directory</param>
		/// <param name="logger">Logger</param>
		/// <param name="cleanDirectory">If true, clean the directory</param>
		/// <returns>Cache state</returns>
		private ArtifactCacheState InitFileCache(DirectoryReference directory, ILogger logger, bool cleanDirectory)
		{
			try
			{
				if (cleanDirectory)
				{
					// Clear the output directory
					try
					{
						Directory.Delete(directory.FullName, true);
					}
					catch (Exception)
					{ }
				}
				Directory.CreateDirectory(directory.FullName);

				_store = new FileStorageClient(directory, logger);

				State = ArtifactCacheState.Available;
				return State;
			}
			catch (Exception)
			{
				State = ArtifactCacheState.Unavailable;
				throw;
			}
		}

		/// <summary>
		/// Return the ref name for horde storage given a artitfact action collection key
		/// </summary>
		/// <param name="key">Artifact action collection key</param>
		/// <returns>The reference name</returns>
		private static RefName GetRefName(IoHash key)
		{
			return new RefName($"action_artifact_v2_{key}");
		}
	}
}
