// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Channels;
using System.Threading.Tasks;
using EpicGames.Core;
using Horde.Agent.Utility;
using HordeCommon;
using HordeCommon.Rpc;
using Microsoft.Extensions.Logging;

namespace Horde.Agent.Parser
{
	using JsonObject = System.Text.Json.Nodes.JsonObject;

	interface IJsonRpcLogSink
	{
		Task WriteEventsAsync(List<CreateEventRequest> events);
		Task WriteOutputAsync(WriteOutputRequest request);
		Task SetOutcomeAsync(JobStepOutcome outcome);
	}

	sealed class JsonRpcLogSink : IJsonRpcLogSink
	{
		readonly IRpcConnection _rpcClient;
		readonly string? _jobId;
		readonly string? _jobBatchId;
		readonly string? _jobStepId;
		readonly ILogger _logger;

		public JsonRpcLogSink(IRpcConnection rpcClient, string? jobId, string? jobBatchId, string? jobStepId, ILogger logger)
		{
			_rpcClient = rpcClient;
			_jobId = jobId;
			_jobBatchId = jobBatchId;
			_jobStepId = jobStepId;
			_logger = logger;
		}

		/// <inheritdoc/>
		public async Task WriteEventsAsync(List<CreateEventRequest> events)
		{
			await _rpcClient.InvokeAsync(x => x.CreateEventsAsync(new CreateEventsRequest(events)), new RpcContext(), CancellationToken.None);
		}

		/// <inheritdoc/>
		public async Task WriteOutputAsync(WriteOutputRequest request)
		{
			await _rpcClient.InvokeAsync(x => x.WriteOutputAsync(request), new RpcContext(), CancellationToken.None);
		}

		public async Task SetOutcomeAsync(JobStepOutcome outcome)
		{
			// Update the outcome of this jobstep
			if (_jobId != null && _jobBatchId != null && _jobStepId != null)
			{
				try
				{
					await _rpcClient.InvokeAsync(x => x.UpdateStepAsync(new UpdateStepRequest(_jobId, _jobBatchId, _jobStepId, JobStepState.Unspecified, outcome)), new RpcContext(), CancellationToken.None);
				}
				catch (Exception ex)
				{
					_logger.LogWarning(ex, "Unable to update step outcome to {NewOutcome}", outcome);
				}
			}
		}
	}

	/// <summary>
	/// Class to handle uploading log data to the server in the background
	/// </summary>
	sealed class JsonRpcLogger : ILogger, IAsyncDisposable
	{
		class QueueItem
		{
			public byte[] Data { get; }
			public CreateEventRequest? CreateEvent { get; }

			public QueueItem(byte[] data, CreateEventRequest? createEvent)
			{
				Data = data;
				CreateEvent = createEvent;
			}

			public override string ToString()
			{
				return Encoding.UTF8.GetString(Data);
			}
		}

		readonly IJsonRpcLogSink _sink;
		readonly string _logId;
		readonly bool _warnings;
		readonly ILogger _inner;
		readonly Channel<JsonLogEvent> _dataChannel;
		Task? _dataWriter;

		/// <summary>
		/// The current outcome for this step. Updated to reflect any errors and warnings that occurred.
		/// </summary>
		public JobStepOutcome Outcome
		{
			get;
			private set;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="sink">Sink for log events</param>
		/// <param name="logId">The log id to write to</param>
		/// <param name="warnings">Whether to include warnings in the output</param>
		/// <param name="inner">Additional logger to write to</param>
		public JsonRpcLogger(IJsonRpcLogSink sink, string logId, bool? warnings, ILogger inner)
		{
			_sink = sink;
			_logId = logId;
			_warnings = warnings ?? true;
			_inner = inner;
			_dataChannel = Channel.CreateUnbounded<JsonLogEvent>();
			_dataWriter = Task.Run(() => RunDataWriter());

			Outcome = JobStepOutcome.Success;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="rpcClient">RPC client to use for server requests</param>
		/// <param name="logId">The log id to write to</param>
		/// <param name="jobId">Id of the job being executed</param>
		/// <param name="jobBatchId">Batch being executed</param>
		/// <param name="jobStepId">Id of the step being executed</param>
		/// <param name="warnings">Whether to include warnings in the output</param>
		/// <param name="inner">Additional logger to write to</param>
		public JsonRpcLogger(IRpcConnection rpcClient, string logId, string? jobId, string? jobBatchId, string? jobStepId, bool? warnings, ILogger inner)
			: this(new JsonRpcLogSink(rpcClient, jobId, jobBatchId, jobStepId, inner), logId, warnings, inner)
		{
		}

		/// <inheritdoc/>
		public void Log<TState>(LogLevel logLevel, EventId eventId, TState state, Exception? exception, Func<TState, Exception?, string> formatter)
		{
			// Downgrade warnings to information if not required
			if (logLevel == LogLevel.Warning && !_warnings)
			{
				logLevel = LogLevel.Information;
			}

			JsonLogEvent jsonLogEvent = JsonLogEvent.FromLoggerState(logLevel, eventId, state, exception, formatter);
			WriteFormattedEvent(jsonLogEvent);
		}

		/// <inheritdoc/>
		public bool IsEnabled(LogLevel logLevel) => _inner.IsEnabled(logLevel);

		/// <inheritdoc/>
		public IDisposable BeginScope<TState>(TState state) => _inner.BeginScope(state);

		private void WriteFormattedEvent(JsonLogEvent jsonLogEvent)
		{
			// Update the state of this job if this is an error status
			LogLevel level = jsonLogEvent.Level;
			if (level == LogLevel.Error || level == LogLevel.Critical)
			{
				Outcome = JobStepOutcome.Failure;
			}
			else if (level == LogLevel.Warning && Outcome != JobStepOutcome.Failure)
			{
				Outcome = JobStepOutcome.Warnings;
			}

			// Write the event
			if (!_dataChannel.Writer.TryWrite(jsonLogEvent))
			{
				throw new InvalidOperationException("Expected unbounded writer to complete immediately");
			}
		}

		/// <summary>
		/// Stops the log writer's background task
		/// </summary>
		/// <returns>Async task</returns>
		public async Task StopAsync()
		{
			if (_dataWriter != null)
			{
				_dataChannel.Writer.TryComplete();
				await _dataWriter;
				_dataWriter = null;
			}
		}

		/// <summary>
		/// Dispose of this object. Call StopAsync() to stop asynchronously.
		/// </summary>
		public async ValueTask DisposeAsync()
		{
			await StopAsync();
		}

		/// <summary>
		/// Upload the log data to the server in the background
		/// </summary>
		/// <returns>Async task</returns>
		async Task RunDataWriter()
		{
			// Current position and line number in the log file
			long offset = 0;
			int lineIndex = 0;

			// Total number of errors and warnings
			const int MaxErrors = 50;
			int numErrors = 0;
			const int MaxWarnings = 50;
			int numWarnings = 0;

			// Buffers for chunks and events read in a single iteration
			ArrayBufferWriter<byte> writer = new ArrayBufferWriter<byte>();
			List<CreateEventRequest> events = new List<CreateEventRequest>();

			// The current jobstep outcome
			JobStepOutcome postedOutcome = JobStepOutcome.Success;

			// Whether we've written the flush command
			for (; ; )
			{
				writer.Clear();
				events.Clear();

				// Save off the current line number for sending to the server
				int initialLineIndex = lineIndex;

				// Get the next data
				Task waitTask = Task.Delay(TimeSpan.FromSeconds(2.0));
				while (writer.WrittenCount < 256 * 1024)
				{
					JsonLogEvent jsonLogEvent;
					if (_dataChannel.Reader.TryRead(out jsonLogEvent))
					{
						int lineCount = WriteEvent(jsonLogEvent, writer);
						if (jsonLogEvent.LineIndex == 0)
						{
							if (jsonLogEvent.Level == LogLevel.Warning && ++numWarnings <= MaxWarnings)
							{
								AddEvent(jsonLogEvent.Data.Span, lineIndex, Math.Max(lineCount, jsonLogEvent.LineCount), EventSeverity.Warning, events);
							}
							else if ((jsonLogEvent.Level == LogLevel.Error || jsonLogEvent.Level == LogLevel.Critical) && ++numErrors <= MaxErrors)
							{
								AddEvent(jsonLogEvent.Data.Span, lineIndex, Math.Max(lineCount, jsonLogEvent.LineCount), EventSeverity.Error, events);
							}
						}
						lineIndex += lineCount;
					}
					else
					{
						Task<bool> readTask = _dataChannel.Reader.WaitToReadAsync().AsTask();
						if (await Task.WhenAny(readTask, waitTask) == waitTask)
						{
							break;
						}
						if (!await readTask)
						{
							break;
						}
					}
				}

				// Upload it to the server
				if (writer.WrittenCount > 0)
				{
					byte[] data = writer.WrittenSpan.ToArray();
					try
					{
						await _sink.WriteOutputAsync(new WriteOutputRequest(_logId, offset, initialLineIndex, data, false));
					}
					catch (Exception ex)
					{
						_inner.LogWarning(ex, "Unable to write data to server (log {LogId}, offset {Offset})", _logId, offset);
					}
					offset += data.Length;
				}

				// Write all the events
				if (events.Count > 0)
				{
					try
					{
						await _sink.WriteEventsAsync(events);
					}
					catch (Exception ex)
					{
						_inner.LogWarning(ex, "Unable to create events");
					}
				}

				// Update the outcome of this jobstep
				if (Outcome != postedOutcome)
				{
					try
					{
						await _sink.SetOutcomeAsync(Outcome);
					}
					catch (Exception ex)
					{
						_inner.LogWarning(ex, "Unable to update step outcome to {NewOutcome}", Outcome);
					}
					postedOutcome = Outcome;
				}

				// Wait for more data to be available
				if (!await _dataChannel.Reader.WaitToReadAsync())
				{
					try
					{
						await _sink.WriteOutputAsync(new WriteOutputRequest(_logId, offset, lineIndex, Array.Empty<byte>(), true));
					}
					catch (Exception ex)
					{
						_inner.LogWarning(ex, "Unable to flush data to server (log {LogId}, offset {Offset})", _logId, offset);
					}
					break;
				}
			}
		}

		static readonly string s_messagePropertyName = LogEventPropertyName.Message.ToString();
		static readonly string s_formatPropertyName = LogEventPropertyName.Format.ToString();
		static readonly string s_linePropertyName = LogEventPropertyName.Line.ToString();
		static readonly string s_lineCountPropertyName = LogEventPropertyName.LineCount.ToString();

		static readonly Utf8String s_newline = "\n";
		static readonly Utf8String s_escapedNewline = "\\n";

		public static int WriteEvent(JsonLogEvent jsonLogEvent, IBufferWriter<byte> writer)
		{
			ReadOnlySpan<byte> span = jsonLogEvent.Data.Span;
			if (jsonLogEvent.LineCount == 1 && span.IndexOf(s_escapedNewline) != -1)
			{
				JsonObject obj = (JsonObject)JsonNode.Parse(span)!;

				JsonValue? formatValue = obj["format"] as JsonValue;
				if (formatValue != null && formatValue.TryGetValue(out string? format))
				{
					return WriteEventWithFormat(obj, format, writer);
				}

				JsonValue? messageValue = obj["message"] as JsonValue;
				if (messageValue != null && messageValue.TryGetValue(out string? message))
				{
					return WriteEventWithMessage(obj, message, writer);
				}
			}

			writer.Write(span);
			writer.Write(s_newline);
			return 1;
		}

		static int WriteEventWithFormat(JsonObject obj, string format, IBufferWriter<byte> writer)
		{
			IEnumerable<KeyValuePair<string, object?>> propertyValueList = Enumerable.Empty<KeyValuePair<string, object?>>();

			// Split all the multi-line properties into separate properties
			JsonObject? properties = obj["properties"] as JsonObject;
			if (properties != null)
			{
				// Get all the current property values
				Dictionary<string, string> propertyValues = new Dictionary<string, string>(StringComparer.Ordinal);
				foreach ((string name, JsonNode? node) in properties)
				{
					string value = String.Empty;
					if (node != null)
					{
						if (node is JsonObject valueObject)
						{
							value = valueObject["$text"]?.ToString() ?? String.Empty;
						}
						else
						{
							value = node.ToString();
						}
					}
					propertyValues[name] = value;
				}

				// Split all the multi-line properties into separate things
				int nameStart = -1;
				for (int idx = 0; idx < format.Length; idx++)
				{
					if (format[idx] == '{')
					{
						nameStart = idx + 1;
					}
					else if (format[idx] == '}' && nameStart != -1)
					{
						string name = format.Substring(nameStart, idx - nameStart);
						if (propertyValues.TryGetValue(name, out string? text))
						{
							int textLineEnd = text.IndexOf('\n', StringComparison.Ordinal);
							if (textLineEnd != -1)
							{
								int lineNum = 0;

								StringBuilder builder = new StringBuilder();
								builder.Append(format, 0, nameStart - 1);

								string delimiter = String.Empty;
								for (int textLineStart = 0; textLineStart < text.Length;)
								{
									string newName = $"{name}${lineNum++}";
									string newLine = text.Substring(textLineStart, textLineEnd - textLineStart);

									// Insert this line
									builder.Append($"{delimiter}{{{newName}}}");
									properties![newName] = newLine;
									propertyValues[newName] = newLine;
									delimiter = "\n";

									// Move to the next line
									textLineStart = ++textLineEnd;
									while (textLineEnd < text.Length && text[textLineEnd] != '\n')
									{
										textLineEnd++;
									}
								}

								builder.Append(format, idx + 1, format.Length - (idx + 1));
								format = builder.ToString();
							}
						}
					}
				}

				// Get the enumerable property list for formatting
				propertyValueList = propertyValues.Select(x => new KeyValuePair<string, object?>(x.Key, x.Value));
			}

			// Finally split the format string into multiple lines
			string[] lines = format.Split('\n');
			for (int idx = 0; idx < lines.Length; idx++)
			{
				string message = MessageTemplate.Render(lines[idx], propertyValueList);
				WriteSingleEvent(obj, message, lines[idx], idx, lines.Length, writer);
			}
			return lines.Length;
		}

		static int WriteEventWithMessage(JsonObject obj, string message, IBufferWriter<byte> writer)
		{
			string[] lines = message.Split('\n');
			for (int idx = 0; idx < lines.Length; idx++)
			{
				WriteSingleEvent(obj, lines[idx], null, idx, lines.Length, writer);
			}
			return lines.Length;
		}

		static void WriteSingleEvent(JsonObject obj, string message, string? format, int line, int lineCount, IBufferWriter<byte> writer)
		{
			obj[s_messagePropertyName] = message;
			if (format != null)
			{
				obj[s_formatPropertyName] = format;
			}
			if (lineCount > 1)
			{
				obj[s_linePropertyName] = line;
				obj[s_lineCountPropertyName] = lineCount;
			}

			using (Utf8JsonWriter jsonWriter = new Utf8JsonWriter(writer))
			{
				obj.WriteTo(jsonWriter);
			}

			writer.Write(s_newline.Span);
		}

		void AddEvent(ReadOnlySpan<byte> span, int lineIndex, int lineCount, EventSeverity severity, List<CreateEventRequest> events)
		{
			try
			{
				events.Add(new CreateEventRequest(severity, _logId, lineIndex, lineCount));
			}
			catch (Exception ex)
			{
				_inner.LogError(ex, "Exception while trying to parse line count from data ({Message})", Encoding.UTF8.GetString(span));
			}
		}
	}
}
