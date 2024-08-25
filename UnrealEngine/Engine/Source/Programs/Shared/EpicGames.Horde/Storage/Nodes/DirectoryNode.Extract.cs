// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.IO;
using System.IO.MemoryMappedFiles;
using System.Linq;
using System.Threading;
using System.Threading.Channels;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage.Bundles;
using EpicGames.Horde.Storage.Bundles.V2;
using Microsoft.Extensions.Logging;

namespace EpicGames.Horde.Storage.Nodes
{
	/// <summary>
	/// Stats reported for copy operations
	/// </summary>
	public interface IExtractStats
	{
		/// <summary>
		/// Number of files that have been copied
		/// </summary>
		int Count { get; }

		/// <summary>
		/// Total size of data to be copied
		/// </summary>
		long Size { get; }

		/// <summary>
		/// Processing speed, in bytes per second
		/// </summary>
		double Rate { get; }
	}

	/// <summary>
	/// Reports progress info back to callers
	/// </summary>
	class ExtractStats : IExtractStats
	{
		readonly object _lockObject = new object();
		readonly Stopwatch _timer = Stopwatch.StartNew();
		readonly IProgress<IExtractStats>? _progress;
		readonly TimeSpan _frequency;
		long _lastTotalSize;
		readonly Queue<double> _rateSamples = new Queue<double>();

		public int Count { get; set; }
		public long Size { get; set; }
		public double Rate { get; set; }

		public ExtractStats(IProgress<IExtractStats>? progress, TimeSpan frequency)
		{
			_progress = progress;
			_frequency = frequency;
		}

		public void Update(int count, long size)
		{
			if (_progress != null)
			{
				lock (_lockObject)
				{
					Count += count;
					Size += size;
					if (_timer.Elapsed > _frequency)
					{
						FlushInternal();
					}
				}
			}
		}

		public void Flush()
		{
			if (_progress != null)
			{
				lock (_lockObject)
				{
					FlushInternal();
				}
			}
		}

		void FlushInternal()
		{
			_rateSamples.Enqueue((Size - _lastTotalSize) / _timer.Elapsed.TotalSeconds);
			while (_rateSamples.Count > Math.Max(1, 10.0 / _frequency.TotalSeconds))
			{
				_rateSamples.Dequeue();
			}

			Rate = _rateSamples.Average();
			_lastTotalSize = Size;

			_progress!.Report(this);
			_timer.Restart();
		}
	}

	/// <summary>
	/// Progress logger for writing copy stats
	/// </summary>
	public class ExtractStatsLogger : IProgress<IExtractStats>
	{
		readonly int _totalCount;
		readonly long _totalSize;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public ExtractStatsLogger(ILogger logger)
			=> _logger = logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public ExtractStatsLogger(int totalCount, long totalSize, ILogger logger)
		{
			_totalCount = totalCount;
			_totalSize = totalSize;
			_logger = logger;
		}

		/// <inheritdoc/>
		public void Report(IExtractStats stats)
		{
			if (_totalCount > 0 && _totalSize > 0)
			{
				_logger.LogInformation("Copied {NumFiles:n0}/{TotalFiles:n0} files ({Size:n1}/{TotalSize:n1}mb, {Rate:n1}mb/s, {Pct}%)", stats.Count, _totalCount, stats.Size / (1024.0 * 1024.0), _totalSize / (1024.0 * 1024.0), stats.Rate / (1024.0 * 1024.0), (int)((Math.Max(stats.Size, 1) * 100) / Math.Max(_totalSize, 1)));
			}
			else if (_totalCount > 0)
			{
				_logger.LogInformation("Copied {NumFiles:n0}/{TotalFiles:n0} files ({Size:n1}mb, {Rate:n1}mb/s)", stats.Count, _totalCount, stats.Size / (1024.0 * 1024.0), stats.Rate / (1024.0 * 1024.0));
			}
			else if (_totalSize > 0)
			{
				_logger.LogInformation("Copied {NumFiles:n0} files ({Size:n1}/{TotalSize:n1}mb, {Rate:n1}mb/s, {Pct}%)", stats.Count, stats.Size / (1024.0 * 1024.0), _totalSize / (1024.0 * 1024.0), stats.Rate / (1024.0 * 1024.0), (int)((Math.Max(stats.Size, 1) * 100) / Math.Max(_totalSize, 1)));
			}
			else
			{
				_logger.LogInformation("Copied {NumFiles:n0} files ({Size:n1}mb, {Rate:n1}mb/s)", stats.Count, stats.Size / (1024.0 * 1024.0), stats.Rate / (1024.0 * 1024.0));
			}
		}
	}

	/// <summary>
	/// Extension methods for extracting data from directory nodes
	/// </summary>
	public static class DirectoryNodeExtract
	{
		/// <summary>
		/// Utility function to allow extracting a packed directory to disk
		/// </summary>
		/// <param name="directoryNode">Directory to update</param>
		/// <param name="directoryInfo"></param>
		/// <param name="logger"></param>
		/// <param name="cancellationToken"></param>
		public static Task CopyToDirectoryAsync(this DirectoryNode directoryNode, DirectoryInfo directoryInfo, ILogger logger, CancellationToken cancellationToken) => CopyToDirectoryAsync(directoryNode, directoryInfo, null, logger, cancellationToken);

		class OutputFile
		{
			public string Path { get; }
			public FileInfo FileInfo { get; }
			public FileEntry FileEntry { get; }

			bool _createdFile;
			int _remainingChunks;

			public OutputFile(string path, FileInfo fileInfo, FileEntry fileEntry)
			{
				Path = path;
				FileInfo = fileInfo;
				FileEntry = fileEntry;
			}

			public int IncrementRemaining() => Interlocked.Increment(ref _remainingChunks);
			public int DecrementRemaining() => Interlocked.Decrement(ref _remainingChunks);

			public FileStream OpenStream()
			{
				lock (FileEntry)
				{
					if (!_createdFile)
					{
						if (FileInfo.Exists)
						{
							if (FileInfo.LinkTarget != null)
							{
								FileInfo.Delete();
							}
							else if (FileInfo.IsReadOnly)
							{
								FileInfo.IsReadOnly = false;
							}
						}
						else
						{
							FileInfo.Directory?.Create();
						}
					}

					FileStream? stream = null;
					try
					{
						stream = FileInfo.Open(FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.ReadWrite);
						if (!_createdFile)
						{
							stream.SetLength(FileEntry.Length);
							_createdFile = true;
						}
						return stream;
					}
					catch
					{
						stream?.Dispose();
						throw;
					}
				}
			}
		}

		record class OutputChunk(OutputFile File, long Offset, long Length, IBlobHandle Handle);

		record class OutputBatch(List<OutputChunk> Chunks);

		// Writes output chunks to a channel. Buffers one chunk until FlushAsync() is called to ensure
		// the remaining chunk reference count doesn't reach zero until the last chunk has been processed.
		class OutputChunkWriter
		{
			public OutputFile OutputFile { get; }

			readonly ChannelWriter<OutputChunk> _chunkWriter;
			OutputChunk? _bufferedChunk;

			public OutputChunkWriter(OutputFile file, ChannelWriter<OutputChunk> chunkWriter)
			{
				OutputFile = file;
				_chunkWriter = chunkWriter;
			}

			public async Task WriteAsync(long offset, long length, IBlobHandle handle, CancellationToken cancellationToken)
			{
				if (_bufferedChunk != null)
				{
					await _chunkWriter.WriteAsync(_bufferedChunk, cancellationToken);
				}

				OutputFile.IncrementRemaining();
				_bufferedChunk = new OutputChunk(OutputFile, offset, length, handle);
			}

			public async Task FlushAsync(CancellationToken cancellationToken)
			{
				if (_bufferedChunk != null)
				{
					await _chunkWriter.WriteAsync(_bufferedChunk, cancellationToken);
					_bufferedChunk = null;
				}
			}
		}

#pragma warning disable IDE0060
		static void TraceBlobRead(string type, string path, IBlobHandle handle, ILogger logger)
		{
			//			logger.LogTrace(KnownLogEvents.Horde_BlobRead, "Blob [{Type,-20}] Path=\"{Path}\", Locator={Locator}", type, path, handle.GetLocator());
		}
#pragma warning restore IDE0060

		/// <summary>
		/// Utility function to allow extracting a packed directory to disk
		/// </summary>
		/// <param name="directoryNode">Directory to update</param>
		/// <param name="directoryInfo">Direcotry to write to</param>
		/// <param name="progress">Sink for progress updates</param>
		/// <param name="logger">Logger for output</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public static Task CopyToDirectoryAsync(this DirectoryNode directoryNode, DirectoryInfo directoryInfo, IProgress<IExtractStats>? progress, ILogger logger, CancellationToken cancellationToken)
		{
			return CopyToDirectoryAsync(directoryNode, directoryInfo, progress, TimeSpan.FromSeconds(5.0), logger, cancellationToken);
		}

		/// <summary>
		/// Utility function to allow extracting a packed directory to disk
		/// </summary>
		/// <param name="directoryNode">Directory to update</param>
		/// <param name="directoryInfo">Direcotry to write to</param>
		/// <param name="progress">Sink for progress updates</param>
		/// <param name="frequency">Frequency for progress updates</param>
		/// <param name="logger">Logger for output</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public static async Task CopyToDirectoryAsync(this DirectoryNode directoryNode, DirectoryInfo directoryInfo, IProgress<IExtractStats>? progress, TimeSpan frequency, ILogger logger, CancellationToken cancellationToken)
		{
			int numTasks = Math.Min(1 + (int)(directoryNode.Length / (16 * 1024 * 1024)), 16);
			logger.LogInformation("Splitting read into {NumThreads} threads", numTasks);

			ExtractStats extractStats = new ExtractStats(progress, frequency);
			using (CancellationTokenSource cancellationSource = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken))
			{
				// Helper method to run a background task and set a cancellation source on error
				async Task RunBackgroundTask(Func<CancellationToken, Task> taskFunc)
				{
					try
					{
						await Task.Run(() => taskFunc(cancellationSource.Token), cancellationSource.Token);
					}
					catch (OperationCanceledException)
					{
						// Ignore
					}
					catch (Exception ex)
					{
						logger.LogError(ex, "Error while extracting data: {Message}", ex.Message);
						cancellationSource.Cancel();
					}
				}

				List<Task> tasks = new List<Task>();

				Channel<OutputChunk> chunks = Channel.CreateBounded<OutputChunk>(new BoundedChannelOptions(128 * 1024) { FullMode = BoundedChannelFullMode.Wait });
				tasks.Add(RunBackgroundTask(ctx => FindOutputChunksRootAsync(directoryInfo, directoryNode, chunks.Writer, logger, ctx)));

				Channel<OutputBatch> batches = Channel.CreateUnbounded<OutputBatch>();
				tasks.Add(RunBackgroundTask(ctx => ReadBatchesAsync(chunks.Reader, batches.Writer, ctx)));

				Channel<OutputFetchedBatch> prefetchBatches = Channel.CreateBounded<OutputFetchedBatch>(new BoundedChannelOptions(128) { FullMode = BoundedChannelFullMode.Wait });
				try
				{
					tasks.Add(RunBackgroundTask(ctx => FetchAsync(batches.Reader, prefetchBatches.Writer, numTasks, ctx)));

					for (int idx = 0; idx < numTasks; idx++)
					{
						tasks.Add(RunBackgroundTask(ctx => WriteAsync(prefetchBatches.Reader, extractStats, logger, ctx)));
					}

					await Task.WhenAll(tasks);
				}
				finally
				{
					while (prefetchBatches.Reader.TryRead(out OutputFetchedBatch? fetchedBatch))
					{
						fetchedBatch.Dispose();
					}
				}
				extractStats.Flush();
			}
		}

		#region Enumerate chunks

		static async Task FindOutputChunksRootAsync(DirectoryInfo rootDir, DirectoryNode node, ChannelWriter<OutputChunk> chunks, ILogger logger, CancellationToken cancellationToken)
		{
			await FindOutputChunksForDirectoryAsync(rootDir, "", node, chunks, logger, cancellationToken);
			chunks.Complete();
		}

		static async Task FindOutputChunksForDirectoryAsync(DirectoryInfo rootDir, string path, DirectoryNode node, ChannelWriter<OutputChunk> chunks, ILogger logger, CancellationToken cancellationToken)
		{
			foreach (FileEntry fileEntry in node.Files)
			{
				string filePath = CombinePaths(path, fileEntry.Name);
				FileInfo fileInfo = new FileInfo(Path.Combine(rootDir.FullName, filePath));
				OutputFile outputFile = new OutputFile(filePath, fileInfo, fileEntry);

				await FindOutputChunksForFileAsync(outputFile, chunks, logger, cancellationToken);
			}

			foreach (DirectoryEntry directoryEntry in node.Directories)
			{
				string subPath = CombinePaths(path, directoryEntry.Name);
				TraceBlobRead("Directory", subPath, directoryEntry.Handle, logger);
				DirectoryNode subDirectoryNode = await directoryEntry.Handle.ReadBlobAsync(cancellationToken);

				await FindOutputChunksForDirectoryAsync(rootDir, subPath, subDirectoryNode, chunks, logger, cancellationToken);
			}
		}

		static async Task FindOutputChunksForFileAsync(OutputFile outputFile, ChannelWriter<OutputChunk> chunks, ILogger logger, CancellationToken cancellationToken)
		{
			OutputChunkWriter outputWriter = new OutputChunkWriter(outputFile, chunks);
			await FindOutputChunksAsync(outputWriter, 0, outputFile.FileEntry.Target, logger, cancellationToken);
			await outputWriter.FlushAsync(cancellationToken);
		}

		static async Task<long> FindOutputChunksAsync(OutputChunkWriter chunkWriter, long offset, ChunkedDataNodeRef dataRef, ILogger logger, CancellationToken cancellationToken)
		{
			if (dataRef.Type == ChunkedDataNodeType.Leaf)
			{
				await chunkWriter.WriteAsync(offset, dataRef.Length, dataRef.Handle, cancellationToken);
				if (dataRef.Length < 0)
				{
					// Backwards compatibility hack for v2 format
					using BlobData data = await dataRef.Handle.ReadBlobDataAsync(cancellationToken);
					return data.Data.Length;
				}
				return dataRef.Length;
			}
			else
			{
				using BlobData data = await dataRef.Handle.ReadBlobDataAsync(cancellationToken);
				TraceBlobRead("Interior", chunkWriter.OutputFile.Path, dataRef.Handle, logger);

				if (data.Type.Guid == LeafChunkedDataNodeConverter.BlobType.Guid)
				{
					await chunkWriter.WriteAsync(offset, dataRef.Length, dataRef.Handle, cancellationToken);
					return data.Data.Length;
				}
				else
				{
					long length = 0;

					InteriorChunkedDataNode interiorNode = BlobSerializer.Deserialize<InteriorChunkedDataNode>(data);
					foreach (ChunkedDataNodeRef childRef in interiorNode.Children)
					{
						length += await FindOutputChunksAsync(chunkWriter, offset + length, childRef, logger, cancellationToken);
					}

					return length;
				}
			}
		}

		#endregion

		#region Group requests by bundles

		record class OutputExport(BundleHandle BundleHandle, int PacketOffset, int ExportIdx, OutputChunk Chunk);
		record class OutputExportBatch(BundleHandle BundleHandle, List<OutputExport> Exports);

		static async Task ReadBatchesAsync(ChannelReader<OutputChunk> chunkReader, ChannelWriter<OutputBatch> batchWriter, CancellationToken cancellationToken)
		{
			const int MaxQueueLength = 200;

			int queueLength = 0;
			Queue<OutputExportBatch> exportBatchQueue = new Queue<OutputExportBatch>();
			Dictionary<BundleHandle, OutputExportBatch> bundleHandleToExportBatch = new Dictionary<BundleHandle, OutputExportBatch>();

			for (; ; )
			{
				// Fill the queue up to the max length
				while (queueLength < MaxQueueLength)
				{
					OutputChunk? chunk;
					if (!chunkReader.TryRead(out chunk))
					{
						if (!await chunkReader.WaitToReadAsync(cancellationToken))
						{
							break;
						}
					}
					else
					{
						OutputExport? outputExport;
						if (!TryGetOutputExport(chunk, out outputExport))
						{
							OutputBatch batch = new OutputBatch(new List<OutputChunk> { chunk });
							await batchWriter.WriteAsync(batch, cancellationToken);
						}
						else
						{
							BundleHandle bundleHandle = outputExport.BundleHandle;
							if (!bundleHandleToExportBatch.TryGetValue(bundleHandle, out OutputExportBatch? existingExportBatch))
							{
								existingExportBatch = new OutputExportBatch(bundleHandle, new List<OutputExport>());
								exportBatchQueue.Enqueue(existingExportBatch);
								bundleHandleToExportBatch.Add(bundleHandle, existingExportBatch);
							}

							existingExportBatch.Exports.Add(outputExport);
							queueLength++;
						}
					}
				}

				// Exit once we've processed everything and can't get any more items to read.
				if (queueLength == 0)
				{
					batchWriter.TryComplete();
					break;
				}

				// Flush the first queue
				OutputExportBatch exportBatch = exportBatchQueue.Dequeue();
				queueLength -= exportBatch.Exports.Count;
				bundleHandleToExportBatch.Remove(exportBatch.BundleHandle);

				List<OutputChunk> chunkBatch = exportBatch.Exports.OrderBy(x => x.PacketOffset).ThenBy(x => x.ExportIdx).Select(x => x.Chunk).ToList();
				await batchWriter.WriteAsync(new OutputBatch(chunkBatch), cancellationToken);
			}
		}

		static bool TryGetOutputExport(OutputChunk chunk, [NotNullWhen(true)] out OutputExport? export)
		{
			if (chunk.Handle.Innermost is ExportHandle exportHandle && exportHandle.Packet is FlushedPacketHandle packetHandle)
			{
				export = new OutputExport(packetHandle.Bundle, packetHandle.PacketOffset, exportHandle.ExportIdx, chunk);
				return true;
			}
			else
			{
				export = null;
				return false;
			}
		}

		#endregion

		#region Fetch data

		record class OutputFetchedChunk(OutputChunk Chunk, BlobData BlobData) : OutputChunk(Chunk), IDisposable
		{
			public void Dispose() => BlobData.Dispose();
		}

		record class OutputFetchedBatch(List<OutputFetchedChunk> Chunks) : IDisposable
		{
			public OutputFetchedBatch() : this(new List<OutputFetchedChunk>()) { }
			public void Dispose() => Chunks.DisposeAll();
		}

		static async Task FetchAsync(ChannelReader<OutputBatch> batchReader, ChannelWriter<OutputFetchedBatch> batchWriter, int numParallel, CancellationToken cancellationToken)
		{
			List<Task> tasks = new List<Task>();
			for (int idx = 0; idx < numParallel; idx++)
			{
				tasks.Add(Task.Run(() => FetchWorkerAsync(batchReader, batchWriter, cancellationToken), cancellationToken));
			}

			try
			{
				await Task.WhenAll(tasks);
			}
			finally
			{
				batchWriter.Complete();
			}
		}

		static async Task FetchWorkerAsync(ChannelReader<OutputBatch> batchReader, ChannelWriter<OutputFetchedBatch> batchWriter, CancellationToken cancellationToken)
		{
			while (await batchReader.WaitToReadAsync(cancellationToken))
			{
				OutputBatch? batch;
				if (batchReader.TryRead(out batch))
				{
#pragma warning disable CA2000 // fetchedBatch may be pushed onto the output channel, which assumes its ownership.
					OutputFetchedBatch fetchedBatch = new OutputFetchedBatch();
					try
					{
						foreach (OutputChunk chunk in batch.Chunks)
						{
							BlobData blobData = await chunk.Handle.ReadBlobDataAsync(cancellationToken);
							fetchedBatch.Chunks.Add(new OutputFetchedChunk(chunk, blobData));
						}
						await batchWriter.WriteAsync(fetchedBatch, cancellationToken);
					}
					catch
					{
						fetchedBatch.Dispose();
					}
#pragma warning restore CA2000
				}
			}
		}

		#endregion

		#region Write to disk

		static async Task WriteAsync(ChannelReader<OutputFetchedBatch> batchReader, ExtractStats stats, ILogger logger, CancellationToken cancellationToken)
		{
			const int WriteBatchSize = 64;
			while (await batchReader.WaitToReadAsync(cancellationToken))
			{
				OutputFetchedBatch? batch;
				while (batchReader.TryRead(out batch))
				{
					try
					{
						List<Task> tasks = new List<Task>();
						foreach (IReadOnlyList<OutputFetchedChunk> group in batch.Chunks.Batch(WriteBatchSize))
						{
							tasks.Add(WriteChunksAsync(group.ToArray(), stats, logger, cancellationToken));
						}
						await Task.WhenAll(tasks);
					}
					finally
					{
						batch.Dispose();
					}
				}
			}
		}

		static async Task WriteChunksAsync(ArraySegment<OutputFetchedChunk> chunks, ExtractStats stats, ILogger logger, CancellationToken cancellationToken)
		{
			for (int chunkIdx = 0; chunkIdx < chunks.Count;)
			{
				OutputFile file = chunks[chunkIdx].File;

				int maxChunkIdx = chunkIdx + 1;
				while (maxChunkIdx < chunks.Count && chunks[maxChunkIdx].File == file)
				{
					maxChunkIdx++;
				}

				try
				{
					await ExtractChunksToFileAsync(file, chunks.Slice(chunkIdx, maxChunkIdx - chunkIdx), stats, logger, cancellationToken);
					//await ExtractChunksToNullAsync(file, chunks.Slice(chunkIdx, maxChunkIdx - chunkIdx), stats, logger, cancellationToken);
				}
				catch (OperationCanceledException)
				{
					throw;
				}
				catch (Exception ex)
				{
					throw new StorageException($"Unable to extract {file?.FileInfo?.FullName}: {ex.Message}", ex);
				}

				chunkIdx = maxChunkIdx;
			}
		}

		static async Task ExtractChunksToFileAsync(OutputFile file, ArraySegment<OutputFetchedChunk> chunks, ExtractStats stats, ILogger logger, CancellationToken cancellationToken)
		{
			// Open the file for the current chunk
			int remainingChunks = 0;
			await using (FileStream stream = file.OpenStream())
			{
				if (file.FileEntry.Length == 0)
				{
					// If this file is empty, don't write anything and just move to the next chunk
					for (int idx = 0; idx < chunks.Count; idx++)
					{
						remainingChunks = file.DecrementRemaining();
					}
				}
				else
				{
					// Process as many chunks as we can for this file
					using MemoryMappedFile memoryMappedFile = MemoryMappedFile.CreateFromFile(stream, null, file.FileEntry.Length, MemoryMappedFileAccess.ReadWrite, HandleInheritability.None, false);
					using MemoryMappedView memoryMappedView = new MemoryMappedView(memoryMappedFile, 0, file.FileEntry.Length);

					for (int chunkIdx = 0; chunkIdx < chunks.Count; chunkIdx++)
					{
						OutputFetchedChunk chunk = chunks[chunkIdx];
						cancellationToken.ThrowIfCancellationRequested();

						// Write this chunk
						TraceBlobRead("Leaf", chunk.File.Path, chunk.Handle, logger);
						chunk.BlobData.Data.CopyTo(memoryMappedView!.GetMemory(chunk.Offset, chunk.BlobData.Data.Length));

						// Update the stats
						remainingChunks = file.DecrementRemaining();
						stats.Update(0, chunk.Length);
					}
				}
			}

			// Set correct permissions on the output file
			if (remainingChunks == 0)
			{
				file.FileInfo.Refresh();
				FileEntry.SetPermissions(file.FileInfo!, file.FileEntry.Flags);
				stats.Update(1, 0);
			}
		}

#pragma warning disable IDE0051
		// Update counters for extracting chunks without writing any data. Useful for profiling bottlenecks in other stages of the pipeline.
		static Task ExtractChunksToNullAsync(OutputFile file, ArraySegment<OutputFetchedChunk> chunks, ExtractStats stats, ILogger logger, CancellationToken cancellationToken)
		{
			_ = logger;
			_ = cancellationToken;

			foreach (OutputFetchedChunk chunk in chunks)
			{
				int remainingChunks = file.DecrementRemaining();
				if (remainingChunks == 0)
				{
					stats.Update(1, chunk.Length);
				}
				else
				{
					stats.Update(0, chunk.Length);
				}
			}
			return Task.CompletedTask;
		}
#pragma warning restore IDE0051

		#endregion

		static string CombinePaths(string basePath, string nextPath)
		{
			if (basePath.Length > 0)
			{
				return $"{basePath}/{nextPath}";
			}
			else
			{
				return nextPath;
			}
		}
	}
}
