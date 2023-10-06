// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Logs;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Nodes;
using Horde.Server.Logs;
using Horde.Server.Perforce;
using Horde.Server.Server;
using Horde.Server.Storage;
using Horde.Server.Utilities;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using MongoDB.Bson;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;

namespace Horde.Server.Commands.Migrate
{
	[Command("migrate", "logs", "Extracts data from a bundle to the local hard drive")]
	internal class MigrateLogsCommand : Command
	{
		class LogFile
		{
			public ObjectId Id { get; set; }
			public RefName RefName { get; set; }
		}

		readonly IConfiguration _configuration;
		readonly ILoggerProvider _loggerProvider;

		public MigrateLogsCommand(IConfiguration configuration, ILoggerProvider loggerProvider)
		{
			_configuration = configuration;
			_loggerProvider = loggerProvider;
		}

		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			using ServiceProvider serviceProvider = Startup.CreateServiceProvider(_configuration, _loggerProvider);

			MongoService mongoService = serviceProvider.GetRequiredService<MongoService>();
			ILogFileCollection logCollection = serviceProvider.GetRequiredService<ILogFileCollection>();
			ILogFileService logService = serviceProvider.GetRequiredService<ILogFileService>();

			StorageService storageService = serviceProvider.GetRequiredService<StorageService>();
			IStorageClient storageClient = await storageService.GetClientAsync(Namespace.Logs, CancellationToken.None);

			byte[] buffer = new byte[1 * 1024];

			ILogFile logFile = (await logCollection.GetLogFileAsync(LogId.Parse("6374195e5f2709066e7257a6"), CancellationToken.None))!;
//			await foreach(ILogFile logFile in logCollection.GetLogFilesAsync())
			{
				if (logFile.RefName == RefName.Empty)
				{
//					TreeWriter writer = new TreeWriter(storageClient);
					int bufferLength = 0;
					int builderLength = 0;

					LogBuilder builder = new LogBuilder(LogFormat.Json, logger);

					using (Stream stream = await logService.OpenRawStreamAsync(logFile, CancellationToken.None))
					{
						for (; ; )
						{
							int readLength = await stream.ReadAsync(buffer.AsMemory(bufferLength));
							if (readLength == 0)
							{
								break;
							}

							bufferLength += readLength;

							int startIdx = 0;
							for (int idx = 0; idx < bufferLength; idx++)
							{
								if (buffer[idx] == (byte)'\n')
								{
									ReadOnlyMemory<byte> line = buffer.AsMemory(startIdx, idx + 1 - startIdx);

									builder.WriteData(line);
									builderLength += line.Length;

									if (builderLength > 1024 * 1024)
									{
//										await builder.FlushAsync(writer, false, CancellationToken.None);
										builderLength = 0;
									}

									startIdx = idx + 1;
								}
							}

							bufferLength -= startIdx;
							buffer.AsSpan(startIdx, bufferLength).CopyTo(buffer.AsSpan());
						}
					}
//					await builder.FlushAsync(writer, true, CancellationToken.None);
				}
			}

			return 0;
		}
	}
}
