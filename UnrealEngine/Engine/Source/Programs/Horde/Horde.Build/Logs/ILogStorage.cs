// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading.Tasks;
using Horde.Build.Logs.Data;
using Horde.Build.Utilities;

namespace Horde.Build.Logs
{
	using LogId = ObjectId<ILogFile>;

	/// <summary>
	/// Caching interface for reading and writing log data.
	/// </summary>
	public interface ILogStorage : IDisposable
	{
		/// <summary>
		/// Attempts to read an index for the given log file
		/// </summary>
		/// <param name="logId">Unique id of the log file</param>
		/// <param name="length">Length of the file covered by the index</param>
		/// <returns>Index for the log file</returns>
		Task<LogIndexData?> ReadIndexAsync(LogId logId, long length);

		/// <summary>
		/// Log file to write an index for
		/// </summary>
		/// <param name="logId">Unique id of the log file</param>
		/// <param name="length">Length of the file covered by the index</param>
		/// <param name="index">The log file index</param>
		/// <returns>Async task</returns>
		Task WriteIndexAsync(LogId logId, long length, LogIndexData index);

		/// <summary>
		/// Retrieves an item from the cache
		/// </summary>
		/// <param name="logId">Unique id of the log file</param>
		/// <param name="offset">Offset of the chunk to read</param>
		/// <param name="lineIndex">First line of the chunk</param>
		/// <returns>Data for the given key, or null if it's not present</returns>
		Task<LogChunkData?> ReadChunkAsync(LogId logId, long offset, int lineIndex);

		/// <summary>
		/// Writes a chunk to storage
		/// </summary>
		/// <param name="logId">Unique id of the log file</param>
		/// <param name="offset">Offset of the chunk to write</param>
		/// <param name="chunkData">Information about the chunk data</param>
		/// <returns>Async task</returns>
		Task WriteChunkAsync(LogId logId, long offset, LogChunkData chunkData);
	}
}
