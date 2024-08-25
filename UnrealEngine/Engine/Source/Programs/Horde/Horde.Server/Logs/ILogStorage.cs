// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading.Tasks;
using EpicGames.Horde.Logs;
using Horde.Server.Logs.Data;

namespace Horde.Server.Logs
{
	/// <summary>
	/// Legacy interface for accessing log data. This is not used for new log files; the agent writes log chunks directly to the storage client.
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
		/// Retrieves an item from the cache
		/// </summary>
		/// <param name="logId">Unique id of the log file</param>
		/// <param name="offset">Offset of the chunk to read</param>
		/// <param name="lineIndex">First line of the chunk</param>
		/// <returns>Data for the given key, or null if it's not present</returns>
		Task<LogChunkData?> ReadChunkAsync(LogId logId, long offset, int lineIndex);
	}
}
