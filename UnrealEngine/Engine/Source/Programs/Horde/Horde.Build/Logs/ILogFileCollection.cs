// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Threading.Tasks;
using Horde.Build.Agents.Sessions;
using Horde.Build.Jobs;
using Horde.Build.Utilities;

namespace Horde.Build.Logs
{
	using JobId = ObjectId<IJob>;
	using LogId = ObjectId<ILogFile>;
	using SessionId = ObjectId<ISession>;

	/// <summary>
	/// Updates a log file chunk
	/// </summary>
	public class CompleteLogChunkUpdate
	{
		/// <summary>
		/// Index of the chunk
		/// </summary>
		public int Index { get; set; }

		/// <summary>
		/// New length for the chunk
		/// </summary>
		public int Length { get; set; }

		/// <summary>
		/// Number of lines in the chunk
		/// </summary>
		public int LineCount { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="index">Index of the chunk</param>
		/// <param name="length">New length for the chunk</param>
		/// <param name="lineCount">Number of lines in the chunk</param>
		public CompleteLogChunkUpdate(int index, int length, int lineCount)
		{
			Index = index;
			Length = length;
			LineCount = lineCount;
		}
	}

	/// <summary>
	/// Wrapper around the jobs collection in a mongo DB
	/// </summary>
	public interface ILogFileCollection
	{
		/// <summary>
		/// Creates a new log
		/// </summary>
		/// <param name="jobId">Unique id of the job that owns this log file</param>
		/// <param name="sessionId">Agent session allowed to update the log</param>
		/// <param name="type">Type of events to be stored in the log</param>
		/// <returns>The new log file document</returns>
		Task<ILogFile> CreateLogFileAsync(JobId jobId, SessionId? sessionId, LogType type);

		/// <summary>
		/// Adds a new chunk
		/// </summary>
		/// <param name="logFileInterface">The current log file</param>
		/// <param name="offset">Offset of the new chunk</param>
		/// <param name="lineIndex">Line index for the start of the chunk</param>
		/// <returns>The updated log file document</returns>
		Task<ILogFile?> TryAddChunkAsync(ILogFile logFileInterface, long offset, int lineIndex);

		/// <summary>
		/// Update the log file with final information about certain chunks
		/// </summary>
		/// <param name="logFileInterface">The current log file</param>
		/// <param name="chunks">Chunks to update. New chunks will be inserted</param>
		/// <returns>The updated log file document</returns>
		Task<ILogFile?> TryCompleteChunksAsync(ILogFile logFileInterface, IEnumerable<CompleteLogChunkUpdate> chunks);

		/// <summary>
		/// Update the log file with final information about the index
		/// </summary>
		/// <param name="logFileInterface">The current log file</param>
		/// <param name="newIndexLength">New length of the index</param>
		/// <returns>The updated log file document</returns>
		Task<ILogFile?> TryUpdateIndexAsync(ILogFile logFileInterface, long newIndexLength);

		/// <summary>
		/// Gets a logfile by ID
		/// </summary>
		/// <param name="logFileId">Unique id of the log file</param>
		/// <returns>The logfile document</returns>
		Task<ILogFile?> GetLogFileAsync(LogId logFileId);

		/// <summary>
		/// Gets all the log files
		/// </summary>
		/// <returns>List of log files</returns>
		Task<List<ILogFile>> GetLogFilesAsync(int? index = null, int? count = null);
	}
}
