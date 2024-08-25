// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Claims;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Acls;
using EpicGames.Horde.Jobs;
using EpicGames.Horde.Logs;
using EpicGames.Horde.Storage;
using Horde.Server.Acls;
using Horde.Server.Agents.Sessions;
using Horde.Server.Issues;
using Horde.Server.Jobs;
using Horde.Server.Server;
using Horde.Server.Storage;
using Horde.Server.Utilities;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Options;
using MongoDB.Bson;

namespace Horde.Server.Logs
{
	/// <summary>
	/// Format for the returned data
	/// </summary>
	public enum LogOutputFormat
	{
		/// <summary>
		/// Plain text
		/// </summary>
		Text,

		/// <summary>
		/// Raw output (text/json)
		/// </summary>
		Raw,
	}

	/// <summary>
	/// Controller for the /api/logs endpoint
	/// </summary>
	[ApiController]
	[Authorize]
	[Route("[controller]")]
	public class LogsController : ControllerBase
	{
		private readonly ILogFileService _logFileService;
		private readonly IIssueCollection _issueCollection;
		private readonly JobService _jobService;
		private readonly StorageService _storageService;
		private readonly IOptionsSnapshot<GlobalConfig> _globalConfig;

		/// <summary>
		/// Constructor
		/// </summary>
		public LogsController(ILogFileService logFileService, IIssueCollection issueCollection, JobService jobService, StorageService storageService, IOptionsSnapshot<GlobalConfig> globalConfig)
		{
			_logFileService = logFileService;
			_issueCollection = issueCollection;
			_jobService = jobService;
			_storageService = storageService;
			_globalConfig = globalConfig;
		}

		/// <summary>
		/// Retrieve metadata about a specific log file
		/// </summary>
		/// <param name="logFileId">Id of the log file to get information about</param>
		/// <param name="filter">Filter for the properties to return</param>
		/// <param name="cancellationToken">Cancellation token for the request</param>
		/// <returns>Information about the requested project</returns>
		[HttpGet]
		[Route("/api/v1/logs/{logFileId}")]
		[ProducesResponseType(typeof(GetLogFileResponse), 200)]
		public async Task<ActionResult<object>> GetLogAsync(LogId logFileId, [FromQuery] PropertyFilter? filter = null, CancellationToken cancellationToken = default)
		{
			ILogFile? logFile = await _logFileService.GetLogFileAsync(logFileId, cancellationToken);
			if (logFile == null)
			{
				return NotFound();
			}
			if (!await AuthorizeAsync(logFile, LogAclAction.ViewLog, User, cancellationToken))
			{
				return Forbid();
			}

			LogMetadata metadata = await _logFileService.GetMetadataAsync(logFile, cancellationToken);
			return new GetLogFileResponse(logFile, metadata).ApplyFilter(filter);
		}

		/// <summary>
		/// Uploads a blob for a log file. See /api/v1/storage/XXX/blobs.
		/// </summary>
		/// <param name="logFileId">Id of the log file to get information about</param>
		/// <param name="file">Data for the blob</param>
		/// <param name="cancellationToken">Cancellation token for the request</param>
		/// <returns>Information about the requested project</returns>
		[HttpPost]
		[Route("/api/v1/logs/{logFileId}/blobs")]
		[ProducesResponseType(typeof(WriteBlobResponse), 200)]
		public async Task<ActionResult<WriteBlobResponse>> WriteLogBlobAsync(LogId logFileId, IFormFile? file, CancellationToken cancellationToken = default)
		{
			ILogFile? logFile = await _logFileService.GetLogFileAsync(logFileId, cancellationToken);
			if (logFile == null)
			{
				return NotFound();
			}
			if (!await AuthorizeAsync(logFile, LogAclAction.WriteLogData, User, cancellationToken))
			{
				return Forbid();
			}

			IStorageBackend storageBackend = _storageService.CreateBackend(Namespace.Logs);
			return await StorageController.WriteBlobAsync(storageBackend, file, $"{logFile.RefName}", cancellationToken);
		}

		/// <summary>
		/// Retrieve raw data for a log file
		/// </summary>
		/// <param name="logFileId">Id of the log file to get information about</param>
		/// <param name="format">Format for the returned data</param>
		/// <param name="fileName">Name of the default filename to download</param>
		/// <param name="download">Whether to download the file rather than display in the browser</param>
		/// <param name="cancellationToken">Cancellation token for the request</param>
		/// <returns>Raw log data for the requested range</returns>
		[HttpGet]
		[Route("/api/v1/logs/{logFileId}/data")]
		public async Task<ActionResult> GetLogDataAsync(
			LogId logFileId,
			[FromQuery] LogOutputFormat format = LogOutputFormat.Raw,
			[FromQuery] string? fileName = null,
			[FromQuery] bool download = false,
			CancellationToken cancellationToken = default)
		{
			ILogFile? logFile = await _logFileService.GetLogFileAsync(logFileId, cancellationToken);
			if (logFile == null)
			{
				return NotFound();
			}
			if (!await AuthorizeAsync(logFile, LogAclAction.ViewLog, User, cancellationToken))
			{
				return Forbid();
			}

			Func<Stream, ActionContext, Task> copyTask;
			if (format == LogOutputFormat.Text && logFile.Type == LogType.Json)
			{
				copyTask = (outputStream, context) => _logFileService.CopyPlainTextStreamAsync(logFile, outputStream, cancellationToken);
			}
			else
			{
				copyTask = (outputStream, context) => _logFileService.CopyRawStreamAsync(logFile, outputStream, cancellationToken);
			}

			return new CustomFileCallbackResult(fileName ?? $"log-{logFileId}.txt", "text/plain", !download, copyTask);
		}

		/// <summary>
		/// Retrieve line data for a logfile
		/// </summary>
		/// <param name="logFileId">Id of the log file to get information about</param>
		/// <param name="index">Index of the first line to retrieve</param>
		/// <param name="count">Number of lines to retrieve</param>
		/// <param name="cancellationToken">Cancellation token for the request</param>
		/// <returns>Information about the requested project</returns>
		[HttpGet]
		[Route("/api/v1/logs/{logFileId}/lines")]
		public async Task<ActionResult> GetLogLinesAsync(LogId logFileId, [FromQuery] int index = 0, [FromQuery] int count = 100, CancellationToken cancellationToken = default)
		{
			ILogFile? logFile = await _logFileService.GetLogFileAsync(logFileId, cancellationToken);
			if (logFile == null)
			{
				return NotFound();
			}
			if (!await AuthorizeAsync(logFile, LogAclAction.ViewLog, User, cancellationToken))
			{
				return Forbid();
			}

			LogMetadata metadata = await _logFileService.GetMetadataAsync(logFile, cancellationToken);

			List<Utf8String> lines = await _logFileService.ReadLinesAsync(logFile, index, count, cancellationToken);
			using (MemoryStream stream = new MemoryStream(lines.Sum(x => x.Length) + (lines.Count * 20)))
			{
				stream.WriteByte((byte)'{');

				stream.Write(Encoding.UTF8.GetBytes($"\"index\":{index},"));
				stream.Write(Encoding.UTF8.GetBytes($"\"count\":{lines.Count},"));
				stream.Write(Encoding.UTF8.GetBytes($"\"maxLineIndex\":{Math.Max(metadata.MaxLineIndex, index + lines.Count)},"));
				stream.Write(Encoding.UTF8.GetBytes($"\"format\":{(logFile.Type == LogType.Json ? "\"JSON\"" : "\"TEXT\"")},"));

				stream.Write(Encoding.UTF8.GetBytes($"\"lines\":["));
				stream.WriteByte((byte)'\n');

				for (int lineIdx = 0; lineIdx < lines.Count; lineIdx++)
				{
					Utf8String line = lines[lineIdx];

					stream.WriteByte((byte)' ');
					stream.WriteByte((byte)' ');

					if (logFile.Type == LogType.Json)
					{
						await stream.WriteAsync(line.Memory, cancellationToken);
					}
					else
					{
						stream.WriteByte((byte)'\"');
						for (int idx = 0; idx < line.Length; idx++)
						{
							byte character = line[idx];
							if (character >= 32 && character <= 126 && character != '\\' && character != '\"')
							{
								stream.WriteByte(character);
							}
							else
							{
								stream.Write(Encoding.UTF8.GetBytes($"\\x{character:x2}"));
							}
						}
						stream.WriteByte((byte)'\"');
					}

					if (lineIdx + 1 < lines.Count)
					{
						stream.WriteByte((byte)',');
					}

					stream.WriteByte((byte)'\n');
				}

				if (logFile.Type == LogType.Json)
				{
					stream.Write(Encoding.UTF8.GetBytes($"]"));
				}

				stream.WriteByte((byte)'}');

				Response.ContentType = "application/json";
				Response.Headers.ContentLength = stream.Length;
				stream.Position = 0;
				await stream.CopyToAsync(Response.Body, cancellationToken);
			}
			return new EmptyResult();
		}

		/// <summary>
		/// Search log data
		/// </summary>
		/// <param name="logFileId">Id of the log file to get information about</param>
		/// <param name="text">Text to search for</param>
		/// <param name="firstLine">First line to search from</param>
		/// <param name="count">Number of results to return</param>
		/// <param name="cancellationToken">Cancellation token for the request</param>
		/// <returns>Raw log data for the requested range</returns>
		[HttpGet]
		[Route("/api/v1/logs/{logFileId}/search")]
		public async Task<ActionResult<SearchLogFileResponse>> SearchLogFileAsync(
			LogId logFileId,
			[FromQuery] string text,
			[FromQuery] int firstLine = 0,
			[FromQuery] int count = 5,
			CancellationToken cancellationToken = default)
		{
			ILogFile? logFile = await _logFileService.GetLogFileAsync(logFileId, cancellationToken);
			if (logFile == null)
			{
				return NotFound();
			}
			if (!await AuthorizeAsync(logFile, LogAclAction.ViewLog, User, cancellationToken))
			{
				return Forbid();
			}

			SearchLogFileResponse response = new SearchLogFileResponse();
			response.Stats = new SearchStats();
			response.Lines = await _logFileService.SearchLogDataAsync(logFile, text, firstLine, count, response.Stats, cancellationToken);
			return response;
		}

		/// <summary>
		/// Retrieve events for a logfile
		/// </summary>
		/// <param name="logFileId">Id of the log file to get information about</param>
		/// <param name="index">Index of the first line to retrieve</param>
		/// <param name="count">Number of lines to retrieve</param>
		/// <param name="cancellationToken">Cancellation token for the request</param>
		/// <returns>Information about the requested project</returns>
		[HttpGet]
		[Route("/api/v1/logs/{logFileId}/events")]
		[ProducesResponseType(typeof(List<GetLogEventResponse>), 200)]
		public async Task<ActionResult<List<GetLogEventResponse>>> GetEventsAsync(LogId logFileId, [FromQuery] int? index = null, [FromQuery] int? count = null, CancellationToken cancellationToken = default)
		{
			ILogFile? logFile = await _logFileService.GetLogFileAsync(logFileId, cancellationToken);
			if (logFile == null)
			{
				return NotFound();
			}
			if (!await AuthorizeAsync(logFile, LogAclAction.ViewLog, User, cancellationToken))
			{
				return Forbid();
			}

			List<ILogEvent> logEvents = await _logFileService.FindEventsAsync(logFile, null, index, count, cancellationToken);

			Dictionary<ObjectId, int?> spanIdToIssueId = new Dictionary<ObjectId, int?>();

			List<GetLogEventResponse> responses = new List<GetLogEventResponse>();
			foreach (ILogEvent logEvent in logEvents)
			{
				ILogEventData logEventData = await _logFileService.GetEventDataAsync(logFile, logEvent.LineIndex, logEvent.LineCount, cancellationToken);

				int? issueId = null;
				if (logEvent.SpanId != null && !spanIdToIssueId.TryGetValue(logEvent.SpanId.Value, out issueId))
				{
					IIssueSpan? span = await _issueCollection.GetSpanAsync(logEvent.SpanId.Value, cancellationToken);
					issueId = span?.IssueId;
					spanIdToIssueId[logEvent.SpanId.Value] = issueId;
				}

				responses.Add(new GetLogEventResponse(logEvent, logEventData, issueId));
			}
			return responses;
		}

		/// <summary>
		/// Determines if the user is authorized to perform an action on a particular template
		/// </summary>
		/// <param name="logFile">The template to check</param>
		/// <param name="action">The action being performed</param>
		/// <param name="user">The principal to authorize</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>True if the action is authorized</returns>
		async Task<bool> AuthorizeAsync(ILogFile logFile, AclAction action, ClaimsPrincipal user, CancellationToken cancellationToken)
		{
			GlobalConfig globalConfig = _globalConfig.Value;
			if (user.HasAdminClaim())
			{
				return true;
			}
			if (logFile.LeaseId != null && user.HasLeaseClaim(logFile.LeaseId.Value))
			{
				return true;
			}
			if (logFile.SessionId != null && LogFileService.AuthorizeForSession(logFile, user))
			{
				return true;
			}
			if (logFile.JobId != JobId.Empty && await _jobService.AuthorizeAsync(logFile.JobId, action, user, globalConfig, cancellationToken))
			{
				return true;
			}
			if (action == LogAclAction.ViewLog && logFile.SessionId != null && globalConfig.Authorize(SessionAclAction.ViewSession, user))
			{
				return true;
			}
			if (globalConfig.Authorize(action, user))
			{
				return true;
			}
			return false;
		}
	}
}
