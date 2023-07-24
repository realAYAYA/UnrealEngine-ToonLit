// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using Horde.Build.Logs.Data;
using Horde.Build.Utilities;

namespace Horde.Build.Logs
{
	using LogId = ObjectId<ILogFile>;

	/// <summary>
	/// Interface for the log file write cache
	/// </summary>
	public interface ILogBuilder
	{
		/// <summary>
		/// Whether the cache should be flushed on shutdown
		/// </summary>
		bool FlushOnShutdown { get; }

		/// <summary>
		/// Append data to a key
		/// </summary>
		/// <param name="logId">The log file id</param>
		/// <param name="chunkOffset">Offset of the chunk within the log file</param>
		/// <param name="writeOffset">Offset to write to</param>
		/// <param name="writeLineIndex">Line index being written</param>
		/// <param name="writeLineCount">Line count being written</param>
		/// <param name="data">Data to be appended</param>
		/// <param name="type">Type of data stored in this log file</param>
		/// <returns>True if the data was appended to the given chunk. False if the chunk has been completed.</returns>
		Task<bool> AppendAsync(LogId logId, long chunkOffset, long writeOffset, int writeLineIndex, int writeLineCount, ReadOnlyMemory<byte> data, LogType type);

		/// <summary>
		/// Finish the current sub chunk
		/// </summary>
		/// <param name="logId">The log file id</param>
		/// <param name="offset">Offset of the chunk within the log file</param>
		/// <returns>Async task</returns>
		Task CompleteSubChunkAsync(LogId logId, long offset);

		/// <summary>
		/// Finish the current chunk
		/// </summary>
		/// <param name="logId">The log file id</param>
		/// <param name="offset">Offset of the chunk within the log file</param>
		/// <returns>Async task</returns>
		Task CompleteChunkAsync(LogId logId, long offset);

		/// <summary>
		/// Remove a complete chunk from the builder
		/// </summary>
		/// <param name="logId">The log file id</param>
		/// <param name="offset">Offset of the chunk within the log file</param>
		/// <returns>Async task</returns>
		Task RemoveChunkAsync(LogId logId, long offset);

		/// <summary>
		/// Gets the current chunk for the given log file
		/// </summary>
		/// <param name="logId">The log file id</param>
		/// <param name="offset">Offset of the chunk within the log file</param>
		/// <param name="lineIndex">Line index of the chunk within the log file</param>
		/// <returns></returns>
		Task<LogChunkData?> GetChunkAsync(LogId logId, long offset, int lineIndex);

		/// <summary>
		/// Touches the timestamps of all the chunks after the given age, and returns them. Used for flushing the builder.
		/// </summary>
		/// <param name="minAge">Minimum age of the chunks to enumerate. If specified, only chunks last modified longer than this period will be returned.</param>
		/// <returns>List of chunks, identified by log id and chunk index</returns>
		Task<List<(LogId, long)>> TouchChunksAsync(TimeSpan minAge);
	}
}
