// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Security.Claims;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Logs;
using EpicGames.Horde.Storage;
using Horde.Server.Acls;
using Horde.Server.Agents.Leases;
using Horde.Server.Agents.Sessions;
using Horde.Server.Jobs;
using Horde.Server.Logs.Data;
using Horde.Server.Storage;
using Horde.Server.Utilities;
using HordeCommon;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using MongoDB.Bson;
using OpenTelemetry.Trace;
using Stream = System.IO.Stream;

namespace Horde.Server.Logs
{
	/// <summary>
	/// Metadata about a log file
	/// </summary>
	public class LogMetadata
	{
		/// <summary>
		/// Length of the log file
		/// </summary>
		public long Length { get; set; }

		/// <summary>
		/// Number of lines in the log file
		/// </summary>
		public int MaxLineIndex { get; set; }
	}

	/// <summary>
	/// Interface for the log file service
	/// </summary>
	public interface ILogFileService
	{
		/// <summary>
		/// Creates a new log
		/// </summary>
		/// <param name="jobId">Unique id of the job that owns this log file</param>
		/// <param name="leaseId">Lease allowed to update the log</param>
		/// <param name="sessionId">Agent session allowed to update the log</param>
		/// <param name="type">Type of events to be stored in the log</param>
		/// <param name="useNewStorageBackend">Whether to use the new storage backend for log data</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <param name="logId">ID of the log file (optional)</param>
		/// <returns>The new log file document</returns>
		Task<ILogFile> CreateLogFileAsync(JobId jobId, LeaseId? leaseId, SessionId? sessionId, LogType type, bool useNewStorageBackend, LogId? logId = null, CancellationToken cancellationToken = default);

		/// <summary>
		/// Gets a logfile by ID
		/// </summary>
		/// <param name="logFileId">Unique id of the log file</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>The logfile document</returns>
		Task<ILogFile?> GetLogFileAsync(LogId logFileId, CancellationToken cancellationToken);

		/// <summary>
		/// Gets a logfile by ID, returning a cached copy if available. This should only be used to retrieve constant properties set at creation, such as the session or job it's associated with.
		/// </summary>
		/// <param name="logFileId">Unique id of the log file</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>The logfile document</returns>
		Task<ILogFile?> GetCachedLogFileAsync(LogId logFileId, CancellationToken cancellationToken);

		/// <summary>
		/// Returns a list of log files
		/// </summary>
		/// <param name="index">Index of the first result to return</param>
		/// <param name="count">Number of results to return</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>List of logfile documents</returns>
		Task<List<ILogFile>> GetLogFilesAsync(int? index = null, int? count = null, CancellationToken cancellationToken = default);

		/// <summary>
		/// Read a set of lines from the given log file
		/// </summary>
		/// <param name="logFile">Log file to read</param>
		/// <param name="index">Index of the first line to read</param>
		/// <param name="count">Maximum number of lines to return</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>List of lines</returns>
		Task<List<Utf8String>> ReadLinesAsync(ILogFile logFile, int index, int count, CancellationToken cancellationToken = default);

		/// <summary>
		/// Writes out chunk data and assigns to a file
		/// </summary>
		/// <param name="logFile">The log file</param>
		/// <param name="offset">Offset within the file of data</param>
		/// <param name="lineIndex">Current line index of the data (need not be the starting of the line)</param>
		/// <param name="data">the data to add</param>
		/// <param name="flush">Whether the current chunk is complete and should be flushed</param>
		/// <param name="maxChunkLength">The maximum chunk length. Defaults to 128kb.</param>
		/// <param name="maxSubChunkLineCount">Maximum number of lines in each sub-chunk.</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns></returns>
		Task<ILogFile?> WriteLogDataAsync(ILogFile logFile, long offset, int lineIndex, ReadOnlyMemory<byte> data, bool flush, int maxChunkLength = 256 * 1024, int maxSubChunkLineCount = 128, CancellationToken cancellationToken = default);

		/// <summary>
		/// Gets metadata about the log file
		/// </summary>
		/// <param name="logFile">The log file to query</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>Metadata about the log file</returns>
		Task<LogMetadata> GetMetadataAsync(ILogFile logFile, CancellationToken cancellationToken);

		/// <summary>
		/// Creates new log events
		/// </summary>
		/// <param name="newEvents">List of events</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>Async task</returns>
		Task CreateEventsAsync(List<NewLogEventData> newEvents, CancellationToken cancellationToken);

		/// <summary>
		/// Find events for a particular log file
		/// </summary>
		/// <param name="logFile">The log file instance</param>
		/// <param name="spanId">Issue span to return events for</param>
		/// <param name="index">Index of the first event to retrieve</param>
		/// <param name="count">Number of events to retrieve</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>List of log events</returns>
		Task<List<ILogEvent>> FindEventsAsync(ILogFile logFile, ObjectId? spanId = null, int? index = null, int? count = null, CancellationToken cancellationToken = default);

		/// <summary>
		/// Adds events to a log span
		/// </summary>
		/// <param name="events">The events to add</param>
		/// <param name="spanId">The span id</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>Async task</returns>
		Task AddSpanToEventsAsync(IEnumerable<ILogEvent> events, ObjectId spanId, CancellationToken cancellationToken = default);

		/// <summary>
		/// Find events for an issue
		/// </summary>
		/// <param name="spanIds">The span ids</param>
		/// <param name="logIds">Log ids to include</param>
		/// <param name="index">Index within the events for results to return</param>
		/// <param name="count">Number of results to return</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>Async task</returns>
		Task<List<ILogEvent>> FindEventsForSpansAsync(IEnumerable<ObjectId> spanIds, LogId[]? logIds, int index, int count, CancellationToken cancellationToken = default);

		/// <summary>
		/// Gets the data for an event
		/// </summary>
		/// <param name="logFile">The log file instance</param>
		/// <param name="lineIndex">Index of the line in the file</param>
		/// <param name="lineCount">Number of lines in the event</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>New event data instance</returns>
		Task<ILogEventData> GetEventDataAsync(ILogFile logFile, int lineIndex, int lineCount, CancellationToken cancellationToken = default);

		/// <summary>
		/// Gets lines from the given log 
		/// </summary>
		/// <param name="logFile">The log file</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>Data for the requested range</returns>
		Task<Stream> OpenRawStreamAsync(ILogFile logFile, CancellationToken cancellationToken = default);

		/// <summary>
		/// Parses a stream of json text and outputs plain text
		/// </summary>
		/// <param name="logFile">The log file to query</param>
		/// <param name="outputStream">Output stream to receive the text data</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>Async text</returns>
		Task CopyPlainTextStreamAsync(ILogFile logFile, Stream outputStream, CancellationToken cancellationToken = default);

		/// <summary>
		/// Search for the specified text in a log file
		/// </summary>
		/// <param name="logFile">The log file to search</param>
		/// <param name="text">Text to search for</param>
		/// <param name="firstLine">Line to start search from</param>
		/// <param name="count">Number of results to return</param>
		/// <param name="stats">Receives stats for the search</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>List of line numbers containing the given term</returns>
		Task<List<int>> SearchLogDataAsync(ILogFile logFile, string text, int firstLine, int count, SearchStats stats, CancellationToken cancellationToken);
	}

	/// <summary>
	/// Extension methods for dealing with log files
	/// </summary>
	public static class LogFileServiceExtensions
	{
		/// <summary>
		/// Parses a stream of json text and outputs plain text
		/// </summary>
		/// <param name="logFileService">The log file service</param>
		/// <param name="logFile">The log file to query</param>
		/// <param name="outputStream">Output stream to receive the text data</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>Async text</returns>
		public static async Task CopyRawStreamAsync(this ILogFileService logFileService, ILogFile logFile, Stream outputStream, CancellationToken cancellationToken)
		{
			await using Stream stream = await logFileService.OpenRawStreamAsync(logFile, cancellationToken);
			await stream.CopyToAsync(outputStream, cancellationToken);
		}
	}
	
	/// <summary>
	/// Wraps functionality for manipulating logs
	/// </summary>
	public sealed class LogFileService : IHostedService, ILogFileService, IDisposable
	{
		private const int MaxConcurrentChunkWrites = 10;

		private readonly Tracer _tracer;
		private readonly ILogger<LogFileService> _logger;
		private readonly ILogFileCollection _logFiles;
		private readonly ILogEventCollection _logEvents;
		private readonly ILogStorage _storage;
		private readonly ILogBuilder _builder;
		private readonly StorageService _storageService;
		private readonly IOptions<ServerSettings> _settings;

		// Lock object for the <see cref="_writeTasks"/> and <see cref="_writeChunks"/> members
		private readonly object _writeLock = new object();
		private readonly List<Task> _writeTasks = new List<Task>();
		private readonly HashSet<(LogId, long)> _writeChunks = new HashSet<(LogId, long)>();
		private readonly IMemoryCache _logFileCache;

		/// <summary>
		/// Streams log data to a caller
		/// </summary>
		class ResponseStream : Stream
		{
			/// <summary>
			/// The log file service that created this stream
			/// </summary>
			readonly LogFileService _logFileService;

			/// <summary>
			/// The log file being read
			/// </summary>
			readonly ILogFile _logFile;

			/// <summary>
			/// Starting offset within the file of the data to return 
			/// </summary>
			readonly long _responseOffset;

			/// <summary>
			/// Length of data to return
			/// </summary>
			readonly long _responseLength;

			/// <summary>
			/// Current offset within the stream
			/// </summary>
			long _currentOffset;

			/// <summary>
			/// The current chunk index
			/// </summary>
			int _chunkIdx;

			/// <summary>
			/// Buffer containing a message for missing data
			/// </summary>
			ReadOnlyMemory<byte> _sourceBuffer;

			/// <summary>
			/// Offset within the source buffer
			/// </summary>
			int _sourcePos;

			/// <summary>
			/// Length of the source buffer being copied from
			/// </summary>
			int _sourceEnd;

			/// <summary>
			/// Constructor
			/// </summary>
			/// <param name="logFileService">The log file service, for q</param>
			/// <param name="logFile"></param>
			/// <param name="offset"></param>
			/// <param name="length"></param>
			public ResponseStream(LogFileService logFileService, ILogFile logFile, long offset, long length)
			{
				_logFileService = logFileService;
				_logFile = logFile;

				_responseOffset = offset;
				_responseLength = length;

				_currentOffset = offset;

				_chunkIdx = logFile.Chunks.GetChunkForOffset(offset);
				_sourceBuffer = null!;
			}

			/// <inheritdoc/>
			public override bool CanRead => true;

			/// <inheritdoc/>
			public override bool CanSeek => false;

			/// <inheritdoc/>
			public override bool CanWrite => false;

			/// <inheritdoc/>
			public override long Length => _responseLength;

			/// <inheritdoc/>
			public override long Position
			{
				get => _currentOffset - _responseOffset;
				set => throw new NotImplementedException();
			}

			/// <inheritdoc/>
			public override void Flush()
			{
			}

			/// <inheritdoc/>
			public override int Read(byte[] buffer, int offset, int count)
			{
				return ReadAsync(buffer, offset, count, CancellationToken.None).Result;
			}

			/// <inheritdoc/>
			public override async Task<int> ReadAsync(byte[] buffer, int offset, int length, CancellationToken cancellationToken)
			{
				return await ReadAsync(buffer.AsMemory(offset, length), cancellationToken);
			}

			/// <inheritdoc/>
			public override async ValueTask<int> ReadAsync(Memory<byte> buffer, CancellationToken cancellationToken)
			{
				int readBytes = 0;
				while (readBytes < buffer.Length)
				{
					if (_sourcePos < _sourceEnd)
					{
						// Try to copy from the current buffer
						int blockSize = Math.Min(_sourceEnd - _sourcePos, buffer.Length - readBytes);
						_sourceBuffer.Slice(_sourcePos, blockSize).Span.CopyTo(buffer.Slice(readBytes).Span);
						_currentOffset += blockSize;
						readBytes += blockSize;
						_sourcePos += blockSize;
					}
					else if (_currentOffset < _responseOffset + _responseLength)
					{
						// Move to the right chunk
						while (_chunkIdx + 1 < _logFile.Chunks.Count && _currentOffset >= _logFile.Chunks[_chunkIdx + 1].Offset)
						{
							_chunkIdx++;
						}

						// Get the chunk data
						ILogChunk chunk = _logFile.Chunks[_chunkIdx];
						LogChunkData chunkData = await _logFileService.ReadChunkAsync(_logFile, _chunkIdx);

						// Figure out which sub-chunk to use
						int subChunkIdx = chunkData.GetSubChunkForOffsetWithinChunk((int)(_currentOffset - chunk.Offset));
						LogSubChunkData subChunkData = chunkData.SubChunks[subChunkIdx];

						// Get the source data
						long subChunkOffset = chunk.Offset + chunkData.SubChunkOffset[subChunkIdx];
						_sourceBuffer = subChunkData.InflateText().Data;
						_sourcePos = (int)(_currentOffset - subChunkOffset);
						_sourceEnd = (int)Math.Min(_sourceBuffer.Length, (_responseOffset + _responseLength) - subChunkOffset);
					}
					else
					{
						// End of the log
						break;
					}
				}
				return readBytes;
			}

			/// <inheritdoc/>
			public override long Seek(long offset, SeekOrigin origin) => throw new NotImplementedException();

			/// <inheritdoc/>
			public override void SetLength(long value) => throw new NotImplementedException();

			/// <inheritdoc/>
			public override void Write(byte[] buffer, int offset, int count) => throw new NotImplementedException();
		}

		/// <summary>
		/// Streams log data to a caller
		/// </summary>
		class NewLoggerResponseStream : Stream
		{
			readonly LogNode _rootNode;

			/// <summary>
			/// Starting offset within the file of the data to return 
			/// </summary>
			readonly long _responseOffset;

			/// <summary>
			/// Length of data to return
			/// </summary>
			readonly long _responseLength;

			/// <summary>
			/// Current offset within the stream
			/// </summary>
			long _currentOffset;

			/// <summary>
			/// The current chunk index
			/// </summary>
			int _chunkIdx;

			/// <summary>
			/// Buffer containing a message for missing data
			/// </summary>
			ReadOnlyMemory<byte> _sourceBuffer;

			/// <summary>
			/// Offset within the source buffer
			/// </summary>
			int _sourcePos;

			/// <summary>
			/// Length of the source buffer being copied from
			/// </summary>
			int _sourceEnd;

			/// <summary>
			/// Constructor
			/// </summary>
			public NewLoggerResponseStream(LogNode rootNode, long offset, long length)
			{
				_rootNode = rootNode;

				_responseOffset = offset;
				_responseLength = length;

				_currentOffset = offset;

				_chunkIdx = rootNode.TextChunkRefs.GetChunkForOffset(offset);
				_sourceBuffer = null!;
			}

			/// <inheritdoc/>
			public override bool CanRead => true;

			/// <inheritdoc/>
			public override bool CanSeek => false;

			/// <inheritdoc/>
			public override bool CanWrite => false;

			/// <inheritdoc/>
			public override long Length => _responseLength;

			/// <inheritdoc/>
			public override long Position
			{
				get => _currentOffset - _responseOffset;
				set => throw new NotImplementedException();
			}

			/// <inheritdoc/>
			public override void Flush()
			{
			}

			/// <inheritdoc/>
			public override int Read(byte[] buffer, int offset, int count)
			{
				return ReadAsync(buffer, offset, count, CancellationToken.None).Result;
			}

			/// <inheritdoc/>
			public override async Task<int> ReadAsync(byte[] buffer, int offset, int length, CancellationToken cancellationToken)
			{
				return await ReadAsync(buffer.AsMemory(offset, length), cancellationToken);
			}

			/// <inheritdoc/>
			public override async ValueTask<int> ReadAsync(Memory<byte> buffer, CancellationToken cancellationToken)
			{
				int readBytes = 0;
				while (readBytes < buffer.Length)
				{
					if (_sourcePos < _sourceEnd)
					{
						// Try to copy from the current buffer
						int blockSize = Math.Min(_sourceEnd - _sourcePos, buffer.Length - readBytes);
						_sourceBuffer.Slice(_sourcePos, blockSize).Span.CopyTo(buffer.Slice(readBytes).Span);
						_currentOffset += blockSize;
						readBytes += blockSize;
						_sourcePos += blockSize;
					}
					else if (_currentOffset < _responseOffset + _responseLength)
					{
						// Move to the right chunk
						while (_chunkIdx + 1 < _rootNode.TextChunkRefs.Count && _currentOffset >= _rootNode.TextChunkRefs[_chunkIdx + 1].Offset)
						{
							_chunkIdx++;
						}

						// Get the chunk data
						LogChunkRef chunk = _rootNode.TextChunkRefs[_chunkIdx];
						LogChunkNode chunkNode = await chunk.ExpandAsync(cancellationToken);

						// Get the source data
						_sourceBuffer = chunkNode.Data;
						_sourcePos = (int)(_currentOffset - chunk.Offset);
						_sourceEnd = (int)Math.Min(_sourceBuffer.Length, (_responseOffset + _responseLength) - chunk.Offset);
					}
					else
					{
						// End of the log
						break;
					}
				}
				return readBytes;
			}

			/// <inheritdoc/>
			public override long Seek(long offset, SeekOrigin origin) => throw new NotImplementedException();

			/// <inheritdoc/>
			public override void SetLength(long value) => throw new NotImplementedException();

			/// <inheritdoc/>
			public override void Write(byte[] buffer, int offset, int count) => throw new NotImplementedException();
		}

		readonly LogTailService _logTailService;
		readonly ITicker _ticker;

		/// <summary>
		/// Constructor
		/// </summary>
		public LogFileService(ILogFileCollection logFiles, ILogEventCollection logEvents, ILogBuilder builder, ILogStorage storage, IClock clock, LogTailService logTailService, StorageService storageService, IOptions<ServerSettings> settings, Tracer tracer, ILogger<LogFileService> logger)
		{
			_logFiles = logFiles;
			_logEvents = logEvents;
			_logFileCache = new MemoryCache(new MemoryCacheOptions());
			_builder = builder;
			_storage = storage;
			_ticker = clock.AddSharedTicker<LogFileService>(TimeSpan.FromSeconds(30.0), TickAsync, logger);
			_logTailService = logTailService;
			_storageService = storageService;
			_settings = settings;
			_tracer = tracer;
			_logger = logger;
		}

		/// <inheritdoc/>
		public Task StartAsync(CancellationToken cancellationToken) => _ticker.StartAsync();

		/// <inheritdoc/>
		public async Task StopAsync(CancellationToken cancellationToken)
		{
			_logger.LogInformation("Stopping log file service");
			if (_builder.FlushOnShutdown)
			{
				await FlushAsync();
			}
			await _ticker.StopAsync();
			_logger.LogInformation("Log service stopped");
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			_logFileCache.Dispose();
			_storage.Dispose();
			_ticker.Dispose();
		}

		/// <inheritdoc/>
		public Task<ILogFile> CreateLogFileAsync(JobId jobId, LeaseId? leaseId, SessionId? sessionId, LogType type, bool useNewStorageBackend, LogId? logId, CancellationToken cancellationToken)
		{
			return _logFiles.CreateLogFileAsync(jobId, leaseId, sessionId, type, useNewStorageBackend, logId, cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<ILogFile?> GetLogFileAsync(LogId logFileId, CancellationToken cancellationToken)
		{
			ILogFile? logFile = await _logFiles.GetLogFileAsync(logFileId, cancellationToken);
			if(logFile != null)
			{
				AddCachedLogFile(logFile);
			}
			return logFile;
		}

		/// <summary>
		/// Adds a log file to the cache
		/// </summary>
		/// <param name="logFile">The log file to cache</param>
		void AddCachedLogFile(ILogFile logFile)
		{
			MemoryCacheEntryOptions options = new MemoryCacheEntryOptions().SetSlidingExpiration(TimeSpan.FromSeconds(30));
			_logFileCache.Set(logFile.Id, logFile, options);
		}


		/// <inheritdoc />
		public async Task<ILogFile?> GetCachedLogFileAsync(LogId logFileId, CancellationToken cancellationToken)
		{
			object? logFile;
			if (!_logFileCache.TryGetValue(logFileId, out logFile))
			{
				logFile = await GetLogFileAsync(logFileId, cancellationToken);
			}
			return (ILogFile?)logFile;
		}

		/// <inheritdoc/>
		public Task<List<ILogFile>> GetLogFilesAsync(int? index = null, int? count = null, CancellationToken cancellationToken = default)
		{
			return _logFiles.GetLogFilesAsync(index, count, cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<List<Utf8String>> ReadLinesAsync(ILogFile logFile, int index, int count, CancellationToken cancellationToken)
		{
			List<Utf8String> lines = new List<Utf8String>();

			if (logFile.UseNewStorageBackend)
			{
				IStorageClient storageClient = await GetStorageClientAsync(cancellationToken);

				int maxIndex = index + count;

				LogNode? root = await storageClient.TryReadNodeAsync<LogNode>(logFile.RefName, cancellationToken: cancellationToken);
				if (root != null)
				{
					int chunkIdx = root.TextChunkRefs.GetChunkForLine(index);
					for (; index < maxIndex && chunkIdx < root.TextChunkRefs.Count; chunkIdx++)
					{
						LogChunkRef chunk = root.TextChunkRefs[chunkIdx];
						LogChunkNode chunkData = await chunk.ExpandAsync(cancellationToken);

						for (; index < maxIndex && index < chunk.LineIndex; index++)
						{
							lines.Add($"Internal error; missing data for line {index}\n");
						}

						for (; index < maxIndex && index < chunk.LineIndex + chunk.LineCount; index++)
						{
							lines.Add(chunkData.GetLine(index - chunk.LineIndex));
						}
					}
				}

				if (!logFile.Complete)
				{
					await _logTailService.EnableTailingAsync(logFile.Id, root?.LineCount ?? 0);
					if (index < maxIndex)
					{
						await _logTailService.ReadAsync(logFile.Id, index, maxIndex - index, lines);
					}
				}
			}
			else
			{
				(_, long minOffset) = await GetLineOffsetAsync(logFile, index, cancellationToken);
				(_, long maxOffset) = await GetLineOffsetAsync(logFile, index + Math.Min(count, Int32.MaxValue - index), cancellationToken);

				byte[] result;
				using (System.IO.Stream stream = await OpenRawStreamAsync(logFile, minOffset, maxOffset - minOffset, cancellationToken))
				{
					result = new byte[stream.Length];
					await stream.ReadFixedSizeDataAsync(result, 0, result.Length);
				}

				int offset = 0;
				for (int idx = 0; idx < result.Length; idx++)
				{
					if (result[idx] == (byte)'\n')
					{
						lines.Add(new Utf8String(result.AsMemory(offset, idx - offset)));
						offset = idx + 1;
					}
				}
			}

			return lines;
		}

		class WriteState
		{
			public long _offset;
			public int _lineIndex;
			public ReadOnlyMemory<byte> _memory;

			public WriteState(long offset, int lineIndex, ReadOnlyMemory<byte> memory)
			{
				_offset = offset;
				_lineIndex = lineIndex;
				_memory = memory;
			}
		}

		/// <inheritdoc/>
		public async Task<ILogFile?> WriteLogDataAsync(ILogFile logFile, long offset, int lineIndex, ReadOnlyMemory<byte> data, bool flush, int maxChunkLength, int maxSubChunkLineCount, CancellationToken cancellationToken)
		{
			using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(LogFileService)}.{nameof(WriteLogDataAsync)}");
			span.SetAttribute("logId", logFile.Id.ToString());
			span.SetAttribute("offset", offset);
			span.SetAttribute("length", data.Length);
			span.SetAttribute("lineIndex", lineIndex);

			// Make sure the data ends in a newline
			if (data.Length > 0 && data.Span[data.Length - 1] != '\n')
			{
				throw new ArgumentException("Log data must consist of a whole number of lines", nameof(data));
			}

			// Make sure the line count is a power of two
			if ((maxSubChunkLineCount & (maxSubChunkLineCount - 1)) != 0)
			{
				throw new ArgumentException("Maximum line count per sub-chunk must be a power of two", nameof(maxSubChunkLineCount));
			}

			// List of the flushed chunks
			List<long> completeOffsets = new List<long>();

			// Add the data to new chunks
			WriteState state = new WriteState(offset, lineIndex, data);
			while (state._memory.Length > 0)
			{
				// Find an existing chunk to append to
				int chunkIdx = logFile.Chunks.GetChunkForOffset(state._offset);
				if (chunkIdx >= 0)
				{
					ILogChunk chunk = logFile.Chunks[chunkIdx];
					if (await WriteLogChunkDataAsync(logFile, chunk, state, completeOffsets, maxChunkLength, maxSubChunkLineCount, cancellationToken))
					{
						continue;
					}
				}

				// Create a new chunk. Ensure that there's a chunk at the start of the file, even if the current write is beyond it.
				ILogFile? newLogFile;
				if (logFile.Chunks.Count == 0)
				{
					newLogFile = await _logFiles.TryAddChunkAsync(logFile, 0, 0, cancellationToken);
				}
				else
				{
					newLogFile = await _logFiles.TryAddChunkAsync(logFile, state._offset, state._lineIndex, cancellationToken);
				}

				// Try to add a new chunk at the new location
				if (newLogFile == null)
				{
					newLogFile = await _logFiles.GetLogFileAsync(logFile.Id, cancellationToken);
					if (newLogFile == null)
					{
						_logger.LogError("Unable to update log file {LogId}", logFile.Id);
						return null;
					}
					logFile = newLogFile;
				}
				else
				{
					// Logger.LogDebug("Added new chunk at offset {Offset} to log {LogId}", State.Offset, LogFile.Id);
					logFile = newLogFile;
				}
			}

			// Flush any pending chunks on this log file
			if (flush)
			{
				foreach(ILogChunk chunk in logFile.Chunks)
				{
					if (chunk.Length == 0 && !completeOffsets.Contains(chunk.Offset))
					{
						await _builder.CompleteChunkAsync(logFile.Id, chunk.Offset);
						completeOffsets.Add(chunk.Offset);
					}
				}
			}

			// Write all the chunks
			if (completeOffsets.Count > 0 || flush)
			{
				ILogFile? newLogFile = await WriteCompleteChunksForLogAsync(logFile, completeOffsets, flush, cancellationToken);
				if (newLogFile == null)
				{
					return null;
				}
				logFile = newLogFile;
			}
			return logFile;
		}

		/// <summary>
		/// Append data to an existing chunk.
		/// </summary>
		/// <param name="logFile">The log file to append to</param>
		/// <param name="chunk">Chunk within the log file to update</param>
		/// <param name="state">Data remaining to be written</param>
		/// <param name="completeOffsets">List of complete chunks</param>
		/// <param name="maxChunkLength">Maximum length of each chunk</param>
		/// <param name="maxSubChunkLineCount">Maximum number of lines in each subchunk</param>
		/// <param name="cancellationToken">Cancellation token for the call</param> 
		/// <returns>True if data was appended to </returns>
		private async Task<bool> WriteLogChunkDataAsync(ILogFile logFile, ILogChunk chunk, WriteState state, List<long> completeOffsets, int maxChunkLength, int maxSubChunkLineCount, CancellationToken cancellationToken)
		{
			// Don't allow data to be appended if the chunk is complete
			if(chunk.Length > 0)
			{
				return false;
			}

			// Otherwise keep appending subchunks
			bool result = false;
			for (; ; )
			{
				// Flush the current sub-chunk if we're on a boundary
				if (state._lineIndex > 0 && (state._lineIndex & (maxSubChunkLineCount - 1)) == 0)
				{
					_logger.LogDebug("Completing log {LogId} chunk offset {Offset} sub-chunk at line {LineIndex}", logFile.Id, chunk.Offset, state._lineIndex);
					await _builder.CompleteSubChunkAsync(logFile.Id, chunk.Offset);
				}

				// Figure out the max length to write to the current chunk
				int maxLength = Math.Min((int)((chunk.Offset + maxChunkLength) - state._offset), state._memory.Length);

				// Figure out the maximum line index for the current sub chunk
				int minLineIndex = state._lineIndex;
				int maxLineIndex = (minLineIndex & ~(maxSubChunkLineCount - 1)) + maxSubChunkLineCount;

				// Append this data
				(int length, int lineCount) = GetWriteLength(state._memory.Span, maxLength, maxLineIndex - minLineIndex, state._offset == chunk.Offset);
				if (length > 0)
				{
					// Append this data
					ReadOnlyMemory<byte> appendData = state._memory.Slice(0, length);
					if (!await _builder.AppendAsync(logFile.Id, chunk.Offset, state._offset, state._lineIndex, lineCount, appendData, logFile.Type))
					{
						break;
					}

					// Update the state
					//Logger.LogDebug("Append to log {LogId} chunk offset {Offset} (LineIndex={LineIndex}, LineCount={LineCount}, Offset={WriteOffset}, Length={WriteLength})", LogFile.Id, Chunk.Offset, State.LineIndex, LineCount, State.Offset, Length);
					state._offset += length;
					state._lineIndex += lineCount;
					state._memory = state._memory.Slice(length);
					result = true;

					// If this is the end of the data, bail out
					if(state._memory.Length == 0)
					{
						break;
					}
				}

				// Flush the sub-chunk if it's full
				if (state._lineIndex < maxLineIndex)
				{
					_logger.LogDebug("Completing chunk for log {LogId} at offset {Offset}", logFile.Id, chunk.Offset);
					await _builder.CompleteChunkAsync(logFile.Id, chunk.Offset);
					completeOffsets.Add(chunk.Offset);
					break;
				}
			}
			return result;
		}

		/// <summary>
		/// Get the amount of data to write from the given span
		/// </summary>
		/// <param name="span">Data to write</param>
		/// <param name="maxLength">Maximum length of the data to write</param>
		/// <param name="maxLineCount">Maximum number of lines to write</param>
		/// <param name="isEmptyChunk">Whether the current chunk is empty</param>
		/// <returns>A tuple consisting of the amount of data to write and number of lines in it</returns>
		private static (int, int) GetWriteLength(ReadOnlySpan<byte> span, int maxLength, int maxLineCount, bool isEmptyChunk)
		{
			int length = 0;
			int lineCount = 0;
			for (int idx = 0; idx < maxLength || isEmptyChunk; idx++)
			{
				if (span[idx] == '\n')
				{
					length = idx + 1;
					lineCount++;
					isEmptyChunk = false;

					if (lineCount >= maxLineCount)
					{
						break;
					}
				}
			}
			return (length, lineCount);
		}

		/// <inheritdoc/>
		public async Task<LogMetadata> GetMetadataAsync(ILogFile logFile, CancellationToken cancellationToken)
		{
			LogMetadata metadata = new LogMetadata();
			if (logFile.UseNewStorageBackend)
			{
				if (logFile.Complete)
				{
					metadata.MaxLineIndex = logFile.LineCount;
				}
				else
				{
					metadata.MaxLineIndex = await _logTailService.GetFullLineCount(logFile.Id, logFile.LineCount);
				}
			}
			else
			{
				if (logFile.Chunks.Count > 0)
				{
					ILogChunk chunk = logFile.Chunks[logFile.Chunks.Count - 1];
					if (logFile.MaxLineIndex == null || chunk.Length == 0)
					{
						LogChunkData chunkData = await ReadChunkAsync(logFile, logFile.Chunks.Count - 1);
						metadata.Length = chunk.Offset + chunkData.Length;
						metadata.MaxLineIndex = chunk.LineIndex + chunkData.LineCount;
					}
					else
					{
						metadata.Length = chunk.Offset + chunk.Length;
						metadata.MaxLineIndex = logFile.MaxLineIndex.Value;
					}
				}
			}
			return metadata;
		}

		/// <inheritdoc/>
		public Task CreateEventsAsync(List<NewLogEventData> newEvents, CancellationToken cancellationToken)
		{
			return _logEvents.AddManyAsync(newEvents);
		}

		/// <inheritdoc/>
		public Task<List<ILogEvent>> FindEventsAsync(ILogFile logFile, ObjectId? spanId = null, int? index = null, int? count = null, CancellationToken cancellationToken = default)
		{
			return _logEvents.FindAsync(logFile.Id, spanId, index, count);
		}

		class LogEventLine : ILogEventLine
		{
			readonly LogLevel _level;
			public EventId? EventId { get; }
			public string Message { get; }
			public JsonElement Data { get; }

			LogLevel ILogEventLine.Level => _level;

			public LogEventLine(ReadOnlySpan<byte> data)
				: this(JsonSerializer.Deserialize<JsonElement>(data))
			{
			}

			public LogEventLine(JsonElement data)
			{
				Data = data;

				JsonElement levelElement;
				if (!data.TryGetProperty("level", out levelElement) || !Enum.TryParse(levelElement.GetString(), out _level))
				{
					_level = LogLevel.Information;
				}

				JsonElement idElement;
				if (data.TryGetProperty("id", out idElement))
				{
					int idValue;
					if (idElement.TryGetInt32(out idValue))
					{
						EventId = idValue;
					}
				}

				JsonElement messageElement;
				if (data.TryGetProperty("renderedMessage", out messageElement) || data.TryGetProperty("message", out messageElement))
				{
					Message = messageElement.GetString() ?? "(Invalid)";
				}
				else
				{
					Message = "(Missing message or renderedMessage field)";
				}
			}
		}

		class LogEventData : ILogEventData
		{
			public IReadOnlyList<ILogEventLine> Lines { get; }

			EventId? ILogEventData.EventId => (Lines.Count > 0) ? Lines[0].EventId : null;
			EventSeverity ILogEventData.Severity => (Lines.Count == 0) ? EventSeverity.Information : (Lines[0].Level == LogLevel.Warning) ? EventSeverity.Warning : EventSeverity.Error;
			string ILogEventData.Message => String.Join("\n", Lines.Select(x => x.Message));

			public LogEventData(IReadOnlyList<ILogEventLine> lines)
			{
				Lines = lines;
			}
		}

		/// <inheritdoc/>
		public Task AddSpanToEventsAsync(IEnumerable<ILogEvent> events, ObjectId spanId, CancellationToken cancellationToken)
		{
			return _logEvents.AddSpanToEventsAsync(events, spanId);
		}

		/// <inheritdoc/>
		public Task<List<ILogEvent>> FindEventsForSpansAsync(IEnumerable<ObjectId> spanIds, LogId[]? logIds, int index, int count, CancellationToken cancellationToken)
		{
			return _logEvents.FindEventsForSpansAsync(spanIds, logIds, index, count);
		}

		/// <inheritdoc/>
		public async Task<ILogEventData> GetEventDataAsync(ILogFile logFile, int lineIndex, int lineCount, CancellationToken cancellationToken)
		{
			using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(LogFileService)}.{nameof(GetEventDataAsync)}");
			span.SetAttribute("logId", logFile.Id.ToString());
			span.SetAttribute("lineIndex", lineIndex);
			span.SetAttribute("lineCount", lineCount);

			List<Utf8String> lines = await ReadLinesAsync(logFile, lineIndex, lineCount, cancellationToken);
			List<LogEventLine> eventLines = new List<LogEventLine>(lines.Count);

			foreach (Utf8String line in lines)
			{
				try
				{
					eventLines.Add(new LogEventLine(line.Span));
				}
				catch (JsonException ex)
				{
					_logger.LogWarning(ex, "Unable to parse line from log file: {Line}", line);
				}
			}

			return new LogEventData(eventLines);
		}

		/// <inheritdoc/>
		public Task<Stream> OpenRawStreamAsync(ILogFile logFile, CancellationToken cancellationToken)
		{
			return OpenRawStreamAsync(logFile, 0, Int64.MaxValue, cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<Stream> OpenRawStreamAsync(ILogFile logFile, long offset, long length, CancellationToken cancellationToken)
		{
			if (logFile.UseNewStorageBackend)
			{
				IStorageClient storageClient = await GetStorageClientAsync(cancellationToken);

				LogNode? root = await storageClient.TryReadNodeAsync<LogNode>(logFile.RefName, cancellationToken: cancellationToken);
				if (root == null || root.TextChunkRefs.Count == 0)
				{
					return new MemoryStream(Array.Empty<byte>(), false);
				}
				else
				{
					int lastChunkIdx = root.TextChunkRefs.Count - 1;

					// Clamp the length of the request
					LogChunkRef lastChunk = root.TextChunkRefs[lastChunkIdx];
					if (length > lastChunk.Offset)
					{
						long lastChunkLength = lastChunk.Length;
						if (lastChunkLength <= 0)
						{
							LogChunkNode lastChunkNode = await lastChunk.ExpandAsync(cancellationToken);
							lastChunkLength = lastChunkNode.Length;
						}
						length = Math.Min(length, (lastChunk.Offset + lastChunkLength) - offset);
					}

					// Create the new stream
					return new NewLoggerResponseStream(root, offset, length);
				}
			}
			else
			{
				if (logFile.Chunks.Count == 0)
				{
					return new MemoryStream(Array.Empty<byte>(), false);
				}
				else
				{
					int lastChunkIdx = logFile.Chunks.Count - 1;

					// Clamp the length of the request
					ILogChunk lastChunk = logFile.Chunks[lastChunkIdx];
					if (length > lastChunk.Offset)
					{
						long lastChunkLength = lastChunk.Length;
						if (lastChunkLength <= 0)
						{
							LogChunkData lastChunkData = await ReadChunkAsync(logFile, lastChunkIdx);
							lastChunkLength = lastChunkData.Length;
						}
						length = Math.Min(length, (lastChunk.Offset + lastChunkLength) - offset);
					}

					// Create the new stream
					return new ResponseStream(this, logFile, offset, length);
				}
			}
		}

		/// <summary>
		/// Helper method for catching exceptions in <see cref="LogText.ConvertToPlainText(ReadOnlySpan{Byte}, Byte[], Int32)"/>
		/// </summary>
		public static int GuardedConvertToPlainText(ReadOnlySpan<byte> input, byte[] output, int outputOffset, ILogger logger)
		{
			try
			{
				return LogText.ConvertToPlainText(input, output, outputOffset);
			}
			catch (Exception ex)
			{
				logger.LogWarning(ex, "Unable to convert log line to plain text: {Line}", Encoding.UTF8.GetString(input));
				output[outputOffset] = (byte)'\n';
				return outputOffset + 1;
			}
		}

		/// <inheritdoc/>
		public async Task CopyPlainTextStreamAsync(ILogFile logFile, Stream outputStream, CancellationToken cancellationToken)
		{
			long offset = 0;
			long length = Int64.MaxValue;

			using (Stream stream = await OpenRawStreamAsync(logFile, 0, Int64.MaxValue, cancellationToken))
			{
				byte[] readBuffer = new byte[4096];
				int readBufferLength = 0;

				byte[] writeBuffer = new byte[4096];
				int writeBufferLength = 0;

				while (length > 0)
				{
					// Add more data to the buffer
					int readBytes = await stream.ReadAsync(readBuffer.AsMemory(readBufferLength, readBuffer.Length - readBufferLength), cancellationToken);
					readBufferLength += readBytes;

					// Copy as many lines as possible to the output
					int convertedBytes = 0;
					for (int endIdx = 1; endIdx < readBufferLength; endIdx++)
					{
						if (readBuffer[endIdx] == '\n')
						{
							writeBufferLength = GuardedConvertToPlainText(readBuffer.AsSpan(convertedBytes, endIdx - convertedBytes), writeBuffer, writeBufferLength, _logger);
							convertedBytes = endIdx + 1;
						}
					}

					// If there's anything in the write buffer, write it out
					if (writeBufferLength > 0)
					{
						if (offset < writeBufferLength)
						{
							int writeLength = (int)Math.Min((long)writeBufferLength - offset, length);
							await outputStream.WriteAsync(writeBuffer.AsMemory((int)offset, writeLength), cancellationToken);
							length -= writeLength;
						}
						offset = Math.Max(offset - writeBufferLength, 0);
						writeBufferLength = 0;
					}

					// If we were able to read something, shuffle down the rest of the buffer. Otherwise expand the read buffer.
					if (convertedBytes > 0)
					{
						Buffer.BlockCopy(readBuffer, convertedBytes, readBuffer, 0, readBufferLength - convertedBytes);
						readBufferLength -= convertedBytes;
					}
					else if (readBufferLength > 0)
					{
						Array.Resize(ref readBuffer, readBuffer.Length + 128);
						writeBuffer = new byte[readBuffer.Length];
					}

					// Exit if we didn't read anything in this iteration
					if (readBytes == 0)
					{
						break;
					}
				}
			}
		}

		async Task<IStorageClient> GetStorageClientAsync(CancellationToken cancellationToken)
		{
			return await _storageService.GetClientAsync(Namespace.Logs, cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<(int, long)> GetLineOffsetAsync(ILogFile logFile, int lineIdx, CancellationToken cancellationToken)
		{
			if (logFile.UseNewStorageBackend)
			{
				IStorageClient storageClient = await GetStorageClientAsync(cancellationToken);

				LogNode? root = await storageClient.TryReadNodeAsync<LogNode>(logFile.RefName, cancellationToken: cancellationToken);
				if (root == null)
				{
					return (0, 0);
				}

				int chunkIdx = root.TextChunkRefs.GetChunkForLine(lineIdx);
				LogChunkRef chunk = root.TextChunkRefs[chunkIdx];
				LogChunkNode chunkData = await chunk.ExpandAsync(cancellationToken);

				if (lineIdx < chunk.LineIndex)
				{
					lineIdx = chunk.LineIndex;
				}

				int maxLineIndex = chunk.LineIndex + chunkData.LineCount;
				if (lineIdx >= maxLineIndex)
				{
					lineIdx = maxLineIndex;
				}

				long offset = chunk.Offset + chunkData.LineOffsets[lineIdx - chunk.LineIndex];
				return (lineIdx, offset);
			}
			else
			{
				int chunkIdx = logFile.Chunks.GetChunkForLine(lineIdx);

				ILogChunk chunk = logFile.Chunks[chunkIdx];
				LogChunkData chunkData = await ReadChunkAsync(logFile, chunkIdx);

				if (lineIdx < chunk.LineIndex)
				{
					lineIdx = chunk.LineIndex;
				}

				int maxLineIndex = chunk.LineIndex + chunkData.LineCount;
				if (lineIdx >= maxLineIndex)
				{
					lineIdx = maxLineIndex;
				}

				long offset = chunk.Offset + chunkData.GetLineOffsetWithinChunk(lineIdx - chunk.LineIndex);
				return (lineIdx, offset);
			}
		}

		/// <summary>
		/// Executes a background task
		/// </summary>
		/// <param name="stoppingToken">Cancellation token</param>
		async ValueTask TickAsync(CancellationToken stoppingToken)
		{
			using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(LogFileService)}.{nameof(TickAsync)}");
			
			lock (_writeLock)
			{
				try
				{
					_writeTasks.RemoveCompleteTasks();
				}
				catch (Exception ex)
				{
					_logger.LogError(ex, "Exception while waiting for write tasks to complete");
				}
			}
			await IncrementalFlushAsync(stoppingToken);
		}
				
		/// <summary>
		/// Flushes complete chunks to the storage provider
		/// </summary>
		/// <returns>Async task</returns>
		private async Task IncrementalFlushAsync(CancellationToken cancellationToken)
		{
			using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(LogFileService)}.{nameof(IncrementalFlushAsync)}");
			
			// Get all the chunks older than 20 minutes
			List<(LogId, long)> flushChunks = await _builder.TouchChunksAsync(TimeSpan.FromMinutes(10.0));

			span.SetAttribute("numChunks", flushChunks.Count);

			// Mark them all as complete
			foreach ((LogId logId, long offset) in flushChunks)
			{
				await _builder.CompleteChunkAsync(logId, offset);
			}

			// Flush all the chunks and await completion instead of running them async
			await WriteCompleteChunksV2Async(flushChunks, true, cancellationToken);
		}

		/// <summary>
		/// Flushes the write cache
		/// </summary>
		/// <returns>Async task</returns>
		public async Task FlushAsync()
		{
			using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(LogFileService)}.{nameof(FlushAsync)}");
			_logger.LogInformation("Forcing flush of pending log chunks...");

			// Mark everything in the cache as complete
			List<(LogId, long)> writeChunks = await _builder.TouchChunksAsync(TimeSpan.Zero);
			WriteCompleteChunks(writeChunks, true);

			// Wait for everything to flush
			await FlushPendingWritesAsync();
		}

		/// <summary>
		/// Flush any writes in progress
		/// </summary>
		/// <returns>Async task</returns>
		public async Task FlushPendingWritesAsync()
		{
			for(; ;)
			{
				// Capture the current contents of the WriteTasks list
				List<Task> tasks;
				lock (_writeLock)
				{
					_writeTasks.RemoveCompleteTasks();
					tasks = new List<Task>(_writeTasks);
				}
				if (tasks.Count == 0)
				{
					break;
				}

				// Also add a delay so we'll periodically refresh the list
				tasks.Add(Task.Delay(TimeSpan.FromSeconds(5.0)));
				await Task.WhenAny(tasks);
			}
		}

		/// <summary>
		/// Adds tasks for writing a list of complete chunks
		/// </summary>
		/// <param name="chunksToWrite">List of chunks to write</param>
		/// <param name="createIndex">Create an index for the log</param>
		private void WriteCompleteChunks(List<(LogId, long)> chunksToWrite, bool createIndex)
		{
			using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(LogFileService)}.{nameof(WriteCompleteChunks)}");

			int numTasksCreated = 0;
			
			foreach (IGrouping<LogId, long> group in chunksToWrite.GroupBy(x => x.Item1, x => x.Item2))
			{
				LogId logId = group.Key;

				// Find offsets of new chunks to write
				List<long> offsets = new List<long>();
				lock (_writeLock)
				{
					foreach (long offset in group.OrderBy(x => x))
					{
						if (_writeChunks.Add((logId, offset)))
						{
							offsets.Add(offset);
						}
					}
				}

				// Create the write task
				if (offsets.Count > 0)
				{
					Task task = Task.Run(() => WriteCompleteChunksForLogAsync(logId, offsets, createIndex));
					numTasksCreated++;
					lock (_writeLock)
					{
						_writeTasks.Add(task);
					}
				}
			}

			span.SetAttribute("numWriteTasksCreated", numTasksCreated);
			_logger.LogInformation("{NumWriteTasksCreated} write tasks created", numTasksCreated);
		}
		
		/// <summary>
		/// Writes list of complete chunks
		/// </summary>
		/// <param name="chunksToWrite">List of chunks to write</param>
		/// <param name="createIndex">Create an index for the log</param>
		/// <param name="cancellationToken">Cancellation token</param>
		private async Task WriteCompleteChunksV2Async(List<(LogId, long)> chunksToWrite, bool createIndex, CancellationToken cancellationToken)
		{
			using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(LogFileService)}.{nameof(WriteCompleteChunksV2Async)}");
			
			HashSet<(LogId, long)> writeChunks = new ();
			List<(LogId, List<long>)> offsetsToWrite = new();
			
			foreach (IGrouping<LogId, long> group in chunksToWrite.GroupBy(x => x.Item1, x => x.Item2))
			{
				LogId logId = group.Key;

				// Find offsets of new chunks to write
				List<long> offsets = new ();
				foreach (long offset in group.OrderBy(x => x))
				{
					if (writeChunks.Add((logId, offset)))
					{
						offsets.Add(offset);
					}
				}
				
				// Create the write task
				if (offsets.Count > 0)
				{
					offsetsToWrite.Add((logId, offsets));
				}
			}

			span.SetAttribute("numOffsetsToWrite", offsetsToWrite.Count);
			ParallelOptions opts = new() { MaxDegreeOfParallelism = MaxConcurrentChunkWrites, CancellationToken = cancellationToken };
			await Parallel.ForEachAsync(offsetsToWrite, opts, async (x, innerCt) =>
			{
				(LogId logId, List<long> offsets) = x;
				await WriteCompleteChunksForLogAsync(logId, offsets, createIndex, innerCt);
			});
		}

		/// <summary>
		/// Writes a set of chunks to the database
		/// </summary>
		/// <param name="logId">Log file to update</param>
		/// <param name="offsets">Chunks to write</param>
		/// <param name="createIndex">Whether to create the index for this log</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>Async task</returns>
		private async Task<ILogFile?> WriteCompleteChunksForLogAsync(LogId logId, List<long> offsets, bool createIndex, CancellationToken cancellationToken = default)
		{
			ILogFile? logFile = await _logFiles.GetLogFileAsync(logId, cancellationToken);
			if(logFile != null)
			{
				logFile = await WriteCompleteChunksForLogAsync(logFile, offsets, createIndex, cancellationToken);
			}
			return logFile;
		}

		/// <summary>
		/// Writes a set of chunks to the database
		/// </summary>
		/// <param name="logFileInterface">Log file to update</param>
		/// <param name="offsets">Chunks to write</param>
		/// <param name="createIndex">Whether to create the index for this log</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>Async task</returns>
		private async Task<ILogFile?> WriteCompleteChunksForLogAsync(ILogFile logFileInterface, List<long> offsets, bool createIndex, CancellationToken cancellationToken = default)
		{
			using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(LogFileService)}.{nameof(WriteCompleteChunksForLogAsync)}");
			span.SetAttribute("logId", logFileInterface.Id.ToString());
			span.SetAttribute("numOffsets", offsets.Count);
			span.SetAttribute("createIndex", createIndex);
			
			// Write the data to the storage provider
			List<Task<LogChunkData?>> chunkWriteTasks = new List<Task<LogChunkData?>>();
			foreach (long offset in offsets)
			{
				int chunkIdx = logFileInterface.Chunks.BinarySearch(x => x.Offset, offset);
				if (chunkIdx >= 0)
				{
					_logger.LogDebug("Queuing write of log {LogId} chunk {ChunkIdx} offset {Offset}", logFileInterface.Id, chunkIdx, offset);
					int lineIndex = logFileInterface.Chunks[chunkIdx].LineIndex;
					chunkWriteTasks.Add(Task.Run(() => WriteChunkAsync(logFileInterface.Id, offset, lineIndex)));
				}
			}
			
			span.SetAttribute("numWriteTasks", chunkWriteTasks.Count);

			// Wait for the tasks to complete, periodically updating the log file object
			ILogFile? logFile = logFileInterface;
			while (chunkWriteTasks.Count > 0)
			{
				// Wait for all tasks to be complete OR (any task has completed AND 30 seconds has elapsed)
				Task allCompleteTask = Task.WhenAll(chunkWriteTasks);
				Task anyCompleteTask = Task.WhenAny(chunkWriteTasks);
				await Task.WhenAny(allCompleteTask, Task.WhenAll(anyCompleteTask, Task.Delay(TimeSpan.FromSeconds(30.0), cancellationToken)));

				// Update the log file with the written chunks
				List<LogChunkData?> writtenChunks = chunkWriteTasks.RemoveCompleteTasks();
				while (logFile != null)
				{
					// Update the length of any complete chunks
					List<CompleteLogChunkUpdate> updates = new List<CompleteLogChunkUpdate>();
					foreach (LogChunkData? chunkData in writtenChunks)
					{
						if (chunkData != null)
						{
							int chunkIdx = logFile.Chunks.GetChunkForOffset(chunkData.Offset);
							if (chunkIdx >= 0)
							{
								ILogChunk chunk = logFile.Chunks[chunkIdx];
								if (chunk.Offset == chunkData.Offset)
								{
									CompleteLogChunkUpdate update = new CompleteLogChunkUpdate(chunkIdx, chunkData.Length, chunkData.LineCount);
									updates.Add(update);
								}
							}
						}
					}

					// Try to apply the updates
					ILogFile? newLogFile = await _logFiles.TryCompleteChunksAsync(logFile, updates, cancellationToken);
					if (newLogFile != null)
					{
						logFile = newLogFile;
						break;
					}

					// Update the log file
					logFile = await GetLogFileAsync(logFile.Id, cancellationToken);
				}
			}

			// Create the index if necessary
			if (createIndex && logFile != null)
			{
				try
				{
					logFile = await CreateIndexAsync(logFile, cancellationToken);
				}
				catch(Exception ex)
				{
					_logger.LogError(ex, "Failed to create index for log {LogId}", logFileInterface.Id);
				}
			}

			return logFile;
		}

		/// <summary>
		/// Creates an index for the given log file
		/// </summary>
		/// <param name="logFile">The log file object</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>Updated log file</returns>
		private async Task<ILogFile?> CreateIndexAsync(ILogFile logFile, CancellationToken cancellationToken)
		{
			if(logFile.Chunks.Count == 0)
			{
				return logFile;
			}

			// Get the new length of the log, and early out if it won't be any longer
			ILogChunk lastChunk = logFile.Chunks[logFile.Chunks.Count - 1];
			if(lastChunk.Offset + lastChunk.Length <= (logFile.IndexLength ?? 0))
			{
				return logFile;
			}

			// Save stats for the index creation
			using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(LogFileService)}.{nameof(CreateIndexAsync)}");
			span.SetAttribute("logId", logFile.Id.ToString());
			span.SetAttribute("length", lastChunk.Offset + lastChunk.Length);

			long newLength = 0;
			int newLineCount = 0;

			// Read the existing index if there is one
			List<LogIndexData> indexes = new List<LogIndexData>();
			if (logFile.IndexLength != null)
			{
				LogIndexData? existingIndex = await ReadIndexAsync(logFile, logFile.IndexLength.Value);
				if(existingIndex != null)
				{
					indexes.Add(existingIndex);
					newLineCount = existingIndex.LineCount;
				}
			}

			// Add all the new chunks
			int chunkIdx = logFile.Chunks.GetChunkForLine(newLineCount);
			if (chunkIdx < 0)
			{
				int firstLine = (logFile.Chunks.Count > 0) ? logFile.Chunks[0].LineIndex : -1;
				throw new Exception($"Invalid chunk index {chunkIdx}. Index.LineCount={newLineCount}, Chunks={logFile.Chunks.Count}, First line={firstLine}");
			}

			for (; chunkIdx < logFile.Chunks.Count; chunkIdx++)
			{
				ILogChunk chunk = logFile.Chunks[chunkIdx];
				LogChunkData chunkData = await ReadChunkAsync(logFile, chunkIdx);

				int subChunkIdx = chunkData.GetSubChunkForLine(Math.Max(newLineCount - chunk.LineIndex, 0));
				if(subChunkIdx < 0)
				{
					throw new Exception($"Invalid subchunk index {subChunkIdx}. Chunk {chunkIdx}/{logFile.Chunks.Count}. Index.LineCount={newLineCount}, Chunk.LineIndex={chunk.LineIndex}, First subchunk {chunkData.SubChunkLineIndex[0]}");
				}

				for (; subChunkIdx < chunkData.SubChunks.Count; subChunkIdx++)
				{
					LogSubChunkData subChunkData = chunkData.SubChunks[subChunkIdx];
					if (subChunkData.LineIndex >= newLineCount)
					{
						try
						{
							indexes.Add(subChunkData.BuildIndex(_logger));
						}
						catch (Exception ex)
						{
							throw new Exception($"Failed to create index block - log {logFile.Id}, chunk {chunkIdx} ({logFile.Chunks.Count}), subchunk {subChunkIdx} ({chunkData.SubChunks.Count}), index lines: {newLineCount}, chunk index: {chunk.LineIndex}, subchunk index: {chunk.LineIndex + chunkData.SubChunkLineIndex[subChunkIdx]}, subchunk count: {subChunkData.LineCount}", ex);
						}

						newLength = subChunkData.Offset + subChunkData.Length;
						newLineCount = subChunkData.LineIndex + subChunkData.LineCount;
					}
				}
			}

			// Try to update the log file
			ILogFile? newLogFile = logFile;
			if (newLength > (logFile.IndexLength ?? 0))
			{
				LogIndexData index = LogIndexData.Merge(indexes);
				_logger.LogDebug("Writing index for log {LogId} covering {Length} (index length {IndexLength})", logFile.Id, newLength, index.GetSerializedSize());

				await WriteIndexAsync(logFile.Id, newLength, index);

				while(newLogFile != null && newLength > (newLogFile.IndexLength ?? 0))
				{
					newLogFile = await _logFiles.TryUpdateIndexAsync(newLogFile, newLength, cancellationToken);
					if(newLogFile != null)
					{
						break;
					}
					newLogFile = await _logFiles.GetLogFileAsync(logFile.Id, cancellationToken);
				}
			}
			return newLogFile;
		}

		/// <summary>
		/// Reads a chunk from storage
		/// </summary>
		/// <param name="logFile">Log file to read from</param>
		/// <param name="chunkIdx">The chunk to read</param>
		/// <returns>Chunk data</returns>
		private async Task<LogChunkData> ReadChunkAsync(ILogFile logFile, int chunkIdx)
		{
			ILogChunk chunk = logFile.Chunks[chunkIdx];

			// Try to read the chunk data from storage
			LogChunkData? chunkData = null;
			try
			{
				// If the chunk is not yet complete, query the log builder
				if (chunk.Length == 0)
				{
					chunkData = await _builder.GetChunkAsync(logFile.Id, chunk.Offset, chunk.LineIndex);
				}

				// Otherwise go directly to the log storage
				chunkData ??= await _storage.ReadChunkAsync(logFile.Id, chunk.Offset, chunk.LineIndex);
			}
			catch (Exception ex)
			{
				_logger.LogError(ex, "Unable to read log {LogId} at offset {Offset}", logFile.Id, chunk.Offset);
			}

			// Get the minimum length and line count for the chunk
			if (chunkIdx + 1 < logFile.Chunks.Count)
			{
				ILogChunk nextChunk = logFile.Chunks[chunkIdx + 1];
				chunkData = await RepairChunkDataAsync(logFile, chunkIdx, chunkData, (int)(nextChunk.Offset - chunk.Offset), nextChunk.LineIndex - chunk.LineIndex, $"before next");
			}
			else
			{
				if (logFile.MaxLineIndex != null && chunk.Length != 0)
				{
					chunkData = await RepairChunkDataAsync(logFile, chunkIdx, chunkData, chunk.Length, logFile.MaxLineIndex.Value - chunk.LineIndex, $"last chunk (max line index = {logFile.MaxLineIndex})");
				}
				else
				{
					chunkData ??= await RepairChunkDataAsync(logFile, chunkIdx, chunkData, 1024, 1, "default");
				}
			}

			return chunkData;
		}

		/// <summary>
		/// Validates the given chunk data, and fix it up if necessary
		/// </summary>
		/// <param name="logFile">The log file instance</param>
		/// <param name="chunkIdx">Index of the chunk within the logfile</param>
		/// <param name="chunkData">The chunk data that was read</param>
		/// <param name="length">Expected length of the data</param>
		/// <param name="lineCount">Expected number of lines in the data</param>
		/// <param name="context">Context string for diagnostic output</param>
		/// <returns>Repaired chunk data</returns>
		async Task<LogChunkData> RepairChunkDataAsync(ILogFile logFile, int chunkIdx, LogChunkData? chunkData, int length, int lineCount, string context)
		{
			int currentLength = 0;
			int currentLineCount = 0;
			if(chunkData != null)
			{
				currentLength = chunkData.Length;
				currentLineCount = chunkData.LineCount;
			}

			if (chunkData == null || currentLength < length || currentLineCount < lineCount)
			{
				_logger.LogWarning("Creating placeholder subchunk for log {LogId} chunk {ChunkIdx} (length {Length} vs expected {ExpLength}, lines {LineCount} vs expected {ExpLineCount}, context {Context})", logFile.Id, chunkIdx, currentLength, length, currentLineCount, lineCount, context);

				List<LogSubChunkData> subChunks = new List<LogSubChunkData>();
				if (chunkData != null && chunkData.Length < length && chunkData.LineCount < lineCount)
				{
					subChunks.AddRange(chunkData.SubChunks);
				}

				LogText text = new LogText();
				text.AppendMissingDataInfo(chunkIdx, logFile.Chunks[chunkIdx].Server, length - currentLength, lineCount - currentLineCount);
				subChunks.Add(new LogSubChunkData(logFile.Type, currentLength, currentLineCount, text));

				ILogChunk chunk = logFile.Chunks[chunkIdx];
				chunkData = new LogChunkData(chunk.Offset, chunk.LineIndex, subChunks);

				try
				{
					await _storage.WriteChunkAsync(logFile.Id, chunk.Offset, chunkData);
				}
				catch (Exception ex)
				{
					_logger.LogWarning(ex, "Unable to put repaired log data for log {LogId} chunk {ChunkIdx}", logFile.Id, chunkIdx);
				}
			}
			return chunkData;
		}

		/// <summary>
		/// Writes a set of chunks to the database
		/// </summary>
		/// <param name="logFileId">Unique id of the log file</param>
		/// <param name="offset">Offset of the chunk to write</param>
		/// <param name="lineIndex">First line index of the chunk</param>
		/// <returns>Chunk daata</returns>
		private async Task<LogChunkData?> WriteChunkAsync(LogId logFileId, long offset, int lineIndex)
		{
			// Write the chunk to storage
			LogChunkData? chunkData = await _builder.GetChunkAsync(logFileId, offset, lineIndex);
			if (chunkData == null)
			{
				_logger.LogDebug("Log {LogId} offset {Offset} not found in log builder", logFileId, offset);
			}
			else
			{
				try
				{
					await _storage.WriteChunkAsync(logFileId, chunkData.Offset, chunkData);
				}
				catch (Exception ex)
				{
					_logger.LogError(ex, "Unable to write log {LogId} at offset {Offset}", logFileId, chunkData.Offset);
				}
			}

			// Remove it from the log builder
			try
			{
				await _builder.RemoveChunkAsync(logFileId, offset);
			}
			catch (Exception ex)
			{
				_logger.LogError(ex, "Unable to remove log {LogId} at offset {Offset} from log builder", logFileId, offset);
			}

			return chunkData;
		}

		/// <summary>
		/// Reads a chunk from storage
		/// </summary>
		/// <param name="logFile">Log file to read from</param>
		/// <param name="length">Length of the log covered by the index</param>
		/// <returns>Chunk data</returns>
		private async Task<LogIndexData?> ReadIndexAsync(ILogFile logFile, long length)
		{
			try
			{
				LogIndexData? index = await _storage.ReadIndexAsync(logFile.Id, length);
				return index;
			}
			catch (Exception ex)
			{
				_logger.LogError(ex, "Unable to read log {LogId} index at length {Length}", logFile.Id, length);
				return null;
			}
		}

		/// <summary>
		/// Writes an index to the database
		/// </summary>
		/// <param name="logFileId">Unique id of the log file</param>
		/// <param name="length">Length of the data covered by the index</param>
		/// <param name="index">Index to write</param>
		/// <returns>Async task</returns>
		private async Task WriteIndexAsync(LogId logFileId, long length, LogIndexData index)
		{
			try
			{
				await _storage.WriteIndexAsync(logFileId, length, index);
			}
			catch(Exception ex)
			{
				_logger.LogError(ex, "Unable to write index for log {LogId}", logFileId);
			}
		}

		/// <summary>
		/// Determines if the user is authorized to perform an action on a particular template
		/// </summary>
		/// <param name="logFile">The template to check</param>
		/// <param name="user">The principal to authorize</param>
		/// <returns>True if the action is authorized</returns>
		public static bool AuthorizeForSession(ILogFile logFile, ClaimsPrincipal user)
		{
			if (logFile.SessionId != null && user.HasSessionClaim(logFile.SessionId.Value))
			{
				return true;
			}
			if (logFile.LeaseId != null && user.HasLeaseClaim(logFile.LeaseId.Value))
			{
				return true;
			}
			return false;
		}

		/// <inheritdoc/>
		public async Task<List<int>> SearchLogDataAsync(ILogFile logFile, string text, int firstLine, int count, SearchStats searchStats, CancellationToken cancellationToken)
		{
			Stopwatch timer = Stopwatch.StartNew();

			using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(LogFileService)}.{nameof(SearchLogDataAsync)}");
			span.SetAttribute("logId", logFile.Id.ToString());
			span.SetAttribute("text", text);
			span.SetAttribute("count", count);

			List<int> results = new List<int>();
			if (count > 0)
			{
				IAsyncEnumerable<int> enumerable = (logFile.UseNewStorageBackend) ?
						SearchLogDataInternalNewAsync(logFile, text, firstLine, searchStats, cancellationToken) :
						SearchLogDataInternalAsync(logFile, text, firstLine, searchStats);

				await using IAsyncEnumerator<int> enumerator = enumerable.GetAsyncEnumerator(cancellationToken);
				while (await enumerator.MoveNextAsync() && results.Count < count)
				{
					results.Add(enumerator.Current);
				}
			}

			_logger.LogDebug("Search for \"{SearchText}\" in log {LogId} found {NumResults}/{MaxResults} results, took {Time}ms ({@Stats})", text, logFile.Id, results.Count, count, timer.ElapsedMilliseconds, searchStats);
			return results;
		}

		async IAsyncEnumerable<int> SearchLogDataInternalNewAsync(ILogFile logFile, string text, int firstLine, SearchStats searchStats, [EnumeratorCancellation] CancellationToken cancellationToken)
		{
			SearchTerm searchText = new SearchTerm(text);
			IStorageClient storageClient = await GetStorageClientAsync(cancellationToken);

			// Search the index
			if (logFile.LineCount > 0)
			{
				LogNode? root = await storageClient.ReadNodeAsync<LogNode>(logFile.RefName, cancellationToken: cancellationToken);
				if(root != null)
				{
					LogIndexNode index = await root.IndexRef.ExpandAsync(cancellationToken);
					await foreach (int lineIdx in index.Search(firstLine, searchText, searchStats, cancellationToken))
					{
						yield return lineIdx;
					}
					if (root.Complete)
					{
						yield break;
					}
					firstLine = root.LineCount;
				}
			}

			// Search any tail data we have
			if (!logFile.Complete)
			{
				for (; ; )
				{
					Utf8String[] lines = await ReadTailAsync(logFile, firstLine, cancellationToken);
					if (lines.Length == 0)
					{
						break;
					}

					for (int idx = 0; idx < lines.Length; idx++)
					{
						if (SearchTerm.FindNextOcurrence(lines[idx].Span, 0, searchText) != -1)
						{
							yield return firstLine + idx;
						}
					}

					firstLine += lines.Length;
				}
			}
		}

		async Task<Utf8String[]> ReadTailAsync(ILogFile logFile, int index, CancellationToken cancellationToken)
		{
			const int BatchSize = 128;

			if (logFile.Complete)
			{
				return Array.Empty<Utf8String>();
			}

			string cacheKey = $"{logFile.Id}@{index}";
			if (_logFileCache.TryGetValue(cacheKey, out Utf8String[]? lines))
			{
				return lines!;
			}

			lines = (await _logTailService.ReadAsync(logFile.Id, index, BatchSize)).ToArray();
			if (logFile.Type == LogType.Json)
			{
				LogChunkBuilder builder = new LogChunkBuilder(lines.Sum(x => x.Length));
				foreach (Utf8String line in lines)
				{
					builder.AppendJsonAsPlainText(line.Span, _logger);
				}
				lines = lines.ToArray();
			}

			if (lines.Length == BatchSize)
			{
				int length = lines.Sum(x => x.Length);
				using (ICacheEntry entry = _logFileCache.CreateEntry(cacheKey))
				{
					entry.SetSlidingExpiration(TimeSpan.FromMinutes(1.0));
					entry.SetSize(length);
					entry.SetValue(lines);
				}
			}

			return lines;
		}

		async IAsyncEnumerable<int> SearchLogDataInternalAsync(ILogFile logFile, string text, int firstLine, SearchStats searchStats)
		{
			SearchText searchText = new SearchText(text);

			// Read the index for this log file
			if (logFile.IndexLength != null)
			{
				LogIndexData? indexData = await ReadIndexAsync(logFile, logFile.IndexLength.Value);
				if(indexData != null && firstLine < indexData.LineCount)
				{
					using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(LogFileService)}.{nameof(SearchLogDataInternalAsync)}.Indexed");
					span.SetAttribute("lineCount", indexData.LineCount);

					foreach(int lineIndex in indexData.Search(firstLine, searchText, searchStats))
					{
						yield return lineIndex;
					}

					firstLine = indexData.LineCount;
				}
			}

			// Manually search through the rest of the log
			int chunkIdx = logFile.Chunks.GetChunkForLine(firstLine);
			for (; chunkIdx < logFile.Chunks.Count; chunkIdx++)
			{
				ILogChunk chunk = logFile.Chunks[chunkIdx];

				// Read the chunk data
				LogChunkData chunkData = await ReadChunkAsync(logFile, chunkIdx);
				if (firstLine < chunkData.LineIndex + chunkData.LineCount)
				{
					// Find the first sub-chunk we're looking for
					int subChunkIdx = 0;
					if (firstLine > chunk.LineIndex)
					{
						subChunkIdx = chunkData.GetSubChunkForLine(firstLine - chunk.LineIndex);
					}

					// Search through the sub-chunks
					for (; subChunkIdx < chunkData.SubChunks.Count; subChunkIdx++)
					{
						LogSubChunkData subChunkData = chunkData.SubChunks[subChunkIdx];
						if (firstLine < subChunkData.LineIndex + subChunkData.LineCount)
						{
							// Create an index containing just this sub-chunk
							LogIndexData index = subChunkData.BuildIndex(_logger);
							foreach (int lineIndex in index.Search(firstLine, searchText, searchStats))
							{
								yield return lineIndex;
							}
						}
					}
				}
			}
		}
	}
}

