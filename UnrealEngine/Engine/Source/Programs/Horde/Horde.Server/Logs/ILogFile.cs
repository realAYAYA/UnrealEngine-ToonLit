// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using EpicGames.Core;
using EpicGames.Horde.Agents.Leases;
using EpicGames.Horde.Agents.Sessions;
using EpicGames.Horde.Jobs;
using EpicGames.Horde.Logs;
using EpicGames.Horde.Storage;

namespace Horde.Server.Logs
{
	/// <summary>
	/// Information about a log file chunk
	/// </summary>
	public interface ILogChunk
	{
		/// <summary>
		/// Offset of the chunk within the log
		/// </summary>
		long Offset { get; }

		/// <summary>
		/// Length of this chunk. If zero, the chunk is still being written to.
		/// </summary>
		int Length { get; }

		/// <summary>
		/// Index of the first line within this chunk. If a line straddles two chunks, this is the index of the split line.
		/// </summary>
		int LineIndex { get; }

		/// <summary>
		/// If the chunk has yet to be pushed to persistent storage, includes the name of the server that is currently storing it.
		/// </summary>
		string? Server { get; }
	}

	/// <summary>
	/// Information about a log file
	/// </summary>
	public interface ILogFile
	{
		/// <summary>
		/// Identifier for the LogFile. Randomly generated.
		/// </summary>
		public LogId Id { get; }

		/// <summary>
		/// Unique id of the job containing this log
		/// </summary>
		public JobId JobId { get; }

		/// <summary>
		/// The lease allowed to write to this log
		/// </summary>
		public LeaseId? LeaseId { get; }

		/// <summary>
		/// The session allowed to write to this log
		/// </summary>
		public SessionId? SessionId { get; }

		/// <summary>
		/// Whether to use the new storage backend for log data
		/// </summary>
		public bool UseNewStorageBackend { get; }

		/// <summary>
		/// Maximum line index in the file
		/// </summary>
		public int? MaxLineIndex { get; }

		/// <summary>
		/// Length of the file which is indexed
		/// </summary>
		public long? IndexLength { get; }

		/// <summary>
		/// Type of data stored in this log 
		/// </summary>
		public LogType Type { get; }

		/// <summary>
		/// Chunks within this file
		/// </summary>
		public IReadOnlyList<ILogChunk> Chunks { get; }

		/// <summary>
		/// Namespace containing the log data
		/// </summary>
		public NamespaceId NamespaceId { get; }

		/// <summary>
		/// Name of the ref used to store data for this log
		/// </summary>
		public RefName RefName { get; }

		/// <summary>
		/// Number of lines (V2 storage backend)
		/// </summary>
		public int LineCount { get; }

		/// <summary>
		/// Whether the log is complete (V2 storage backend)
		/// </summary>
		public bool Complete { get; }
	}

	/// <summary>
	/// Extension methods for log files
	/// </summary>
	public static class LogFileExtensions
	{
		/// <summary>
		/// Gets the chunk index containing the given offset.
		/// </summary>
		/// <param name="chunks">The chunks to search</param>
		/// <param name="offset">The offset to search for</param>
		/// <returns>The chunk index containing the given offset</returns>
		public static int GetChunkForOffset(this IReadOnlyList<ILogChunk> chunks, long offset)
		{
			int chunkIndex = chunks.BinarySearch(x => x.Offset, offset);
			if (chunkIndex < 0)
			{
				chunkIndex = ~chunkIndex - 1;
			}
			return chunkIndex;
		}

		/// <summary>
		/// Gets the starting chunk index for the given line
		/// </summary>
		/// <param name="chunks">The chunks to search</param>
		/// <param name="lineIndex">Index of the line to query</param>
		/// <returns>Index of the chunk to fetch</returns>
		public static int GetChunkForLine(this IReadOnlyList<ILogChunk> chunks, int lineIndex)
		{
			int chunkIndex = chunks.BinarySearch(x => x.LineIndex, lineIndex);
			if (chunkIndex < 0)
			{
				chunkIndex = ~chunkIndex - 1;
			}
			return chunkIndex;
		}
	}
}
