// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using EpicGames.Horde.Logs;

namespace Horde.Server.Logs
{
	/// <summary>
	/// The type of data stored in this log file
	/// </summary>
	public enum LogType
	{
		/// <summary>
		/// Plain text data
		/// </summary>
		Text,

		/// <summary>
		/// Structured json objects, output as one object per line (without trailing commas)
		/// </summary>
		Json
	}

	/// <summary>
	/// Creates a new log file
	/// </summary>
	public class CreateLogFileRequest
	{
		/// <summary>
		/// Type of the log file
		/// </summary>
		public LogType Type { get; set; } = LogType.Json;
	}

	/// <summary>
	/// Response from creating a log file
	/// </summary>
	public class CreateLogFileResponse
	{
		/// <summary>
		/// Identifier for the created log file
		/// </summary>
		public string Id { get; set; } = String.Empty;
	}

	/// <summary>
	/// Response describing a log file
	/// </summary>
	public class GetLogFileResponse
	{
		/// <summary>
		/// Unique id of the log file
		/// </summary>
		public string Id { get; set; }

		/// <summary>
		/// Unique id of the job for this log file
		/// </summary>
		public string JobId { get; set; }

		/// <summary>
		/// The lease allowed to write to this log
		/// </summary>
		public string? LeaseId { get; }

		/// <summary>
		/// The session allowed to write to this log
		/// </summary>
		public string? SessionId { get; }

		/// <summary>
		/// Type of events stored in this log
		/// </summary>
		public LogType Type { get; set; }

		/// <summary>
		/// Number of lines in the file
		/// </summary>
		public int LineCount { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="logFile">The logfile to construct from</param>
		/// <param name="metadata">Metadata about the log file</param>
		public GetLogFileResponse(ILogFile logFile, LogMetadata metadata)
		{
			Id = logFile.Id.ToString();
			JobId = logFile.JobId.ToString();
			LeaseId = logFile.LeaseId.ToString();
			SessionId = logFile.SessionId.ToString();
			Type = logFile.Type;
			LineCount = metadata.MaxLineIndex;
		}
	}

	/// <summary>
	/// Response describing a log file
	/// </summary>
	public class SearchLogFileResponse
	{
		/// <summary>
		/// List of line numbers containing the search text
		/// </summary>
		public List<int> Lines { get; set; } = new List<int>();

		/// <summary>
		/// Stats for the search
		/// </summary>
		public SearchStats? Stats { get; set; }
	}
}

