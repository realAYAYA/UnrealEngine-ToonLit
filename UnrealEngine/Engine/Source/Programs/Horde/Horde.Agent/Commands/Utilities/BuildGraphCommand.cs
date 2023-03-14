// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Horde.Agent.Parser;
using HordeCommon;
using HordeCommon.Rpc;
using Microsoft.Extensions.Logging;

namespace Horde.Agent.Commands.Utilities
{
	/// <summary>
	/// Runs a BuildGraph script and captures the processed log output
	/// </summary>
	[Command("BuildGraph", "Executes a BuildGraph script with the given arguments using a build of UAT within the current branch, and runs the output through the log processor")]
	class BuildGraphCommand : Command
	{
		sealed class LogSink : IJsonRpcLogSink, IDisposable
		{
			readonly ILogger _inner;
			readonly FileStream _eventStream;
			readonly FileStream _outputStream;

			public LogSink(DirectoryReference baseDir, ILogger inner)
			{
				_inner = inner;

				FileReference eventsFile = FileReference.Combine(baseDir, "horde-events.txt");
				_eventStream = FileReference.Open(eventsFile, FileMode.Create);
				inner.LogInformation("Writing events to {File}", eventsFile);

				FileReference outputFile = FileReference.Combine(baseDir, "horde-output.txt");
				_outputStream = FileReference.Open(outputFile, FileMode.Create);
				inner.LogInformation("Writing output to {File}", outputFile);
			}

			public void Dispose()
			{
				_eventStream.Dispose();
				_outputStream.Dispose();
			}

			public Task SetOutcomeAsync(JobStepOutcome outcome) => Task.CompletedTask;

			public async Task WriteEventsAsync(List<CreateEventRequest> events)
			{
				foreach (CreateEventRequest request in events)
				{
					JsonSerializerOptions options = new JsonSerializerOptions();
					options.Converters.Add(new JsonStringEnumConverter());
					await JsonSerializer.SerializeAsync(_eventStream, request, options);
					_eventStream.Write(Encoding.UTF8.GetBytes(Environment.NewLine));
				}
			}

			public async Task WriteOutputAsync(WriteOutputRequest request)
			{
				PrintJson(request.Data.Span);
				await _outputStream.WriteAsync(request.Data.Memory);
			}

			void PrintJson(ReadOnlySpan<byte> span)
			{
				while (span.Length > 0)
				{
					int idx = span.IndexOf((byte)'\n');
					if(idx == -1)
					{
						break;
					}
					_inner.LogInformation("{Line}", Encoding.UTF8.GetString(span.Slice(0, idx)));
					span = span.Slice(idx + 1);
				}
			}
		}

		readonly List<string> _arguments = new List<string>();

		/// <inheritdoc/>
		public override void Configure(CommandLineArguments arguments, ILogger logger)
		{
			for(int idx = 0; idx < arguments.Count; idx++)
			{
				if (!arguments.HasBeenUsed(idx))
				{
					_arguments.Add(arguments[idx]);
					arguments.MarkAsUsed(idx);
				}
			}
		}

		/// <inheritdoc/>
		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			DirectoryReference baseDir = DirectoryReference.GetCurrentDirectory();

			FileReference runUatBat;
			for (; ; )
			{
				runUatBat = FileReference.Combine(baseDir, "RunUAT.bat");
				if (FileReference.Exists(runUatBat))
				{
					break;
				}

				DirectoryReference? nextDir = baseDir.ParentDirectory;
				if(nextDir == null)
				{
					logger.LogError("Unable to find RunUAT.bat in the current path");
					return 1;
				}

				baseDir = nextDir;
			}

			using (LogSink sink = new LogSink(runUatBat.Directory, logger))
			{
				await using (JsonRpcLogger jsonLogger = new JsonRpcLogger(sink, "log", null, logger))
				{
					using (LogParser filter = new LogParser(jsonLogger, new List<string>()))
					{
						using (ManagedProcessGroup processGroup = new ManagedProcessGroup())
						{
							Dictionary<string, string> newEnvironment = ManagedProcess.GetCurrentEnvVars();
							newEnvironment["UE_STDOUT_JSON"] = "1";

							List<string> allArguments = new List<string>();
							allArguments.Add(runUatBat.FullName);
							allArguments.Add("BuildGraph");
							allArguments.AddRange(_arguments);

							string fileName = Environment.GetEnvironmentVariable("COMSPEC") ?? "cmd.exe";
							string commandLine = $"/C \"{CommandLineArguments.Join(allArguments)}\"";
							using (ManagedProcess process = new ManagedProcess(processGroup, fileName, commandLine, runUatBat.Directory.FullName, newEnvironment, null, ProcessPriorityClass.Normal))
							{
								await process.CopyToAsync((buffer, offset, length) => filter.WriteData(buffer.AsMemory(offset, length)), 4096, CancellationToken.None);
								process.WaitForExit();
							}
						}
						filter.Flush();
					}
				}
			}
			return 0;
		}
	}
}
