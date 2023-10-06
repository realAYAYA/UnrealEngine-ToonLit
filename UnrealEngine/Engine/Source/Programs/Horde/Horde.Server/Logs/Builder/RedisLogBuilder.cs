// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Redis;
using Horde.Server.Logs.Data;
using Horde.Server.Utilities;
using Microsoft.Extensions.Logging;
using OpenTelemetry.Trace;
using StackExchange.Redis;

namespace Horde.Server.Logs.Builder
{
	using Condition = StackExchange.Redis.Condition;

	/// <summary>
	/// Redis-based cache for log file chunks
	/// </summary>
	class RedisLogBuilder : ILogBuilder
	{
		const string ItemsKey = "log-items";

		struct ChunkKeys
		{
			public string Prefix { get; }
			public string Type => $"{Prefix}-type";
			public string LineIndex => $"{Prefix}-lineindex";
			public string Length => $"{Prefix}-length";
			public string ChunkData => $"{Prefix}-chunk";
			public string SubChunkData => $"{Prefix}-subchunk";
			public string Complete => $"{Prefix}-complete";

			public ChunkKeys(LogId logId, long offset)
			{
				Prefix = $"log-{logId}-chunk-{offset}-builder";
			}

			public static bool TryParse(string prefix, out LogId logId, out long offset)
			{
				Match match = Regex.Match(prefix, "log-([^-]+)-chunk-([^-]+)-builder");
				if (!match.Success || !LogId.TryParse(match.Groups[1].Value, out logId) || !Int64.TryParse(match.Groups[2].Value, out offset))
				{
					logId = LogId.Empty;
					offset = 0;
					return false;
				}
				return true;
			}
		}

		/// <inheritdoc/>
		public bool FlushOnShutdown => false;

		/// <summary>
		/// The Redis database connection pool
		/// </summary>
		readonly RedisConnectionPool _redisConnectionPool;

		/// <summary>
		/// Tracer
		/// </summary>
		readonly Tracer _tracer;
		
		/// <summary>
		/// Logger for debug output
		/// </summary>
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="redisConnectionPool">The redis database singleton</param>
		/// <param name="tracer">Tracer</param>
		/// <param name="logger">Logger for debug output</param>
		public RedisLogBuilder(RedisConnectionPool redisConnectionPool, Tracer tracer, ILogger logger)
		{
			_redisConnectionPool = redisConnectionPool;
			_tracer = tracer;
			_logger = logger;
		}

		/// <inheritdoc/>
		public async Task<bool> AppendAsync(LogId logId, long chunkOffset, long writeOffset, int writeLineIndex, int writeLineCount, ReadOnlyMemory<byte> data, LogType type)
		{
			IDatabase redisDb = _redisConnectionPool.GetDatabase();
			ChunkKeys keys = new ChunkKeys(logId, chunkOffset);

			if(chunkOffset == writeOffset)
			{
				using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(RedisLogBuilder)}.{nameof(AppendAsync)}.CreateChunk");
				span.SetAttribute("logId", logId.ToString());
				span.SetAttribute("offset", chunkOffset);
				span.SetAttribute("writeOffset", writeOffset);

				ITransaction createTransaction = redisDb.CreateTransaction();
				createTransaction.AddCondition(Condition.SortedSetNotContains(ItemsKey, keys.Prefix));
				_ = createTransaction.SortedSetAddAsync(ItemsKey, keys.Prefix, DateTime.UtcNow.Ticks, flags: CommandFlags.FireAndForget);
				_ = createTransaction.StringSetAsync(keys.Type, (int)type, flags: CommandFlags.FireAndForget);
				_ = createTransaction.StringSetAsync(keys.Length, data.Length, flags: CommandFlags.FireAndForget);
				_ = createTransaction.StringSetAsync(keys.LineIndex, writeLineIndex + writeLineCount, flags: CommandFlags.FireAndForget);
				_ = createTransaction.StringSetAsync(keys.SubChunkData, data, flags: CommandFlags.FireAndForget);
				if (await createTransaction.ExecuteAsync())
				{
					_logger.LogTrace("Created {Size} bytes in {Key}", data.Length, keys.SubChunkData);
					return true;
				}
			}

			{
				using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(RedisLogBuilder)}.{nameof(AppendAsync)}.AppendChunk");
				span.SetAttribute("logId", logId.ToString());
				span.SetAttribute("offset", chunkOffset);
				span.SetAttribute("writeOffset", writeOffset);

				ITransaction appendTransaction = redisDb.CreateTransaction();
				appendTransaction.AddCondition(Condition.SortedSetContains(ItemsKey, keys.Prefix));
				appendTransaction.AddCondition(Condition.KeyNotExists(keys.Complete));
				appendTransaction.AddCondition(Condition.StringEqual(keys.Type, (int)type));
				appendTransaction.AddCondition(Condition.StringEqual(keys.Length, (int)(writeOffset - chunkOffset)));
				_ = appendTransaction.StringAppendAsync(keys.SubChunkData, data, flags: CommandFlags.FireAndForget);
				_ = appendTransaction.StringSetAsync(keys.Length, (int)(writeOffset - chunkOffset) + data.Length, flags: CommandFlags.FireAndForget);
				_ = appendTransaction.StringSetAsync(keys.LineIndex, writeLineIndex + writeLineCount, flags: CommandFlags.FireAndForget);
				if (await appendTransaction.ExecuteAsync())
				{
					_logger.LogTrace("Appended {Size} to {Key}", data.Length, keys.SubChunkData);
					return true;
				}
			}

			return false;
		}

		/// <inheritdoc/>
		public async Task CompleteSubChunkAsync(LogId logId, long offset)
		{
			IDatabase redisDb = _redisConnectionPool.GetDatabase();
			ChunkKeys keys = new ChunkKeys(logId, offset);
			
			const int MaxRetries = 10;
			int numTries;
			for (numTries = 0; numTries < MaxRetries; numTries++)
			{
				using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(RedisLogBuilder)}.{nameof(CompleteSubChunkAsync)}");
				span.SetAttribute("logId", logId.ToString());
				span.SetAttribute("offset", offset);

				LogType type = (LogType)(int)await redisDb.StringGetAsync(keys.Type);
				int length = (int)await redisDb.StringGetAsync(keys.Length);
				int lineIndex = (int)await redisDb.StringGetAsync(keys.LineIndex);
				long chunkDataLength = await redisDb.StringLengthAsync(keys.ChunkData);

				RedisValue subChunkTextValue = await redisDb.StringGetAsync(keys.SubChunkData);
				if (subChunkTextValue.IsNullOrEmpty)
				{
					return;
				}

				ReadOnlyLogText subChunkText = new ReadOnlyLogText(subChunkTextValue);
				LogSubChunkData subChunkData = new LogSubChunkData(type, offset + length - subChunkText.Data.Length, lineIndex - subChunkText.LineCount, subChunkText);

				byte[] subChunkDataBytes;
				try
				{
					subChunkDataBytes = subChunkData.ToByteArray(_logger);
				}
				catch (Exception ex)
				{
					throw new LogTextParseException($"Error while parsing sub-chunk for {logId} offset {offset}", ex);
				}

				ITransaction writeTransaction = redisDb.CreateTransaction();

				writeTransaction.AddCondition(Condition.StringLengthEqual(keys.ChunkData, chunkDataLength));
				writeTransaction.AddCondition(Condition.StringLengthEqual(keys.SubChunkData, subChunkText.Length));
				writeTransaction.AddCondition(Condition.StringEqual(keys.Length, length));
				writeTransaction.AddCondition(Condition.StringEqual(keys.LineIndex, lineIndex));
				Task<long> newLength = writeTransaction.StringAppendAsync(keys.ChunkData, subChunkDataBytes);
				_ = writeTransaction.KeyDeleteAsync(keys.SubChunkData, flags: CommandFlags.FireAndForget);

				if (await writeTransaction.ExecuteAsync())
				{
					_logger.LogDebug("Completed sub-chunk for log {LogId} chunk offset {Offset} -> sub-chunk size {SubChunkSize}, chunk size {ChunkSize}", logId, offset, subChunkDataBytes.Length, await newLength);
					return;
				}
				await writeTransaction.WaitAndIgnoreCancellations(newLength);

				// Cool down before retrying
				await Task.Delay(100);
			}

			_logger.LogError("Unable to complete sub-chunk for {LogId} at {Offset} after {NumTries} tries", logId, offset, numTries);
			throw new Exception($"Unable to complete sub-chunk for {logId} at {offset} after {numTries} tries");
		}

		/// <inheritdoc/>
		public async Task CompleteChunkAsync(LogId logId, long offset)
		{
			IDatabase redisDb = _redisConnectionPool.GetDatabase();
			
			using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(RedisLogBuilder)}.{nameof(CompleteChunkAsync)}");
			span.SetAttribute("logId", logId.ToString());
			span.SetAttribute("offset", offset);

			ChunkKeys keys = new ChunkKeys(logId, offset);

			ITransaction transaction = redisDb.CreateTransaction();
			transaction.AddCondition(Condition.SortedSetContains(ItemsKey, keys.Prefix));
			_ = transaction.StringSetAsync(keys.Complete, true, flags: CommandFlags.FireAndForget);
			if(!await transaction.ExecuteAsync())
			{
				_logger.LogDebug("Log {LogId} chunk offset {Offset} is not in Redis builder", logId, offset);
				return;
			}

			await CompleteSubChunkAsync(logId, offset);
		}

		/// <inheritdoc/>
		public async Task RemoveChunkAsync(LogId logId, long offset)
		{
			IDatabase redisDb = _redisConnectionPool.GetDatabase();
			using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(RedisLogBuilder)}.{nameof(RemoveChunkAsync)}");
			span.SetAttribute("logId", logId.ToString());
			span.SetAttribute("offset", offset);

			ChunkKeys keys = new ChunkKeys(logId, offset);

			ITransaction transaction = redisDb.CreateTransaction();
			_ = transaction.KeyDeleteAsync(keys.Type, flags: CommandFlags.FireAndForget);
			_ = transaction.KeyDeleteAsync(keys.LineIndex, flags: CommandFlags.FireAndForget);
			_ = transaction.KeyDeleteAsync(keys.Length, flags: CommandFlags.FireAndForget);
			_ = transaction.KeyDeleteAsync(keys.ChunkData, flags: CommandFlags.FireAndForget);
			_ = transaction.KeyDeleteAsync(keys.SubChunkData, flags: CommandFlags.FireAndForget);
			_ = transaction.KeyDeleteAsync(keys.Complete, flags: CommandFlags.FireAndForget);

			_ = transaction.SortedSetRemoveAsync(ItemsKey, keys.Prefix, flags: CommandFlags.FireAndForget);
			await transaction.ExecuteAsync();
		}

		/// <inheritdoc/>
		public async Task<LogChunkData?> GetChunkAsync(LogId logId, long offset, int lineIndex)
		{
			IDatabase redisDb = _redisConnectionPool.GetDatabase();
			ChunkKeys keys = new ChunkKeys(logId, offset);
			const int MaxRetries = 10;
			int numTries;
			for (numTries = 0; numTries < MaxRetries; numTries++)
			{
				using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(RedisLogBuilder)}.{nameof(GetChunkAsync)}");
				span.SetAttribute("logId", logId.ToString());
				span.SetAttribute("offset", offset);

				RedisValue chunkDataValue = await redisDb.StringGetAsync(keys.ChunkData);

				ReadOnlyMemory<byte> chunkData = chunkDataValue;

				ITransaction transaction = redisDb.CreateTransaction();
				transaction.AddCondition(Condition.StringLengthEqual(keys.ChunkData, chunkData.Length));
				Task<RedisValue> typeTask = transaction.StringGetAsync(keys.Type);
				Task<RedisValue> lastSubChunkDataTask = transaction.StringGetAsync(keys.SubChunkData);

				if (await transaction.ExecuteAsync())
				{
					MemoryReader reader = new MemoryReader(chunkData);

					long subChunkOffset = offset;
					int subChunkLineIndex = lineIndex;

					List<LogSubChunkData> subChunks = new List<LogSubChunkData>();
					while (reader.RemainingMemory.Length > 0)
					{
						LogSubChunkData subChunkData = reader.ReadLogSubChunkData(subChunkOffset, subChunkLineIndex);
						subChunkOffset += subChunkData.Length;
						subChunkLineIndex += subChunkData.LineCount;
						subChunks.Add(subChunkData);
					}

					RedisValue type = await typeTask;

					RedisValue lastSubChunkData = await lastSubChunkDataTask;
					if (lastSubChunkData.Length() > 0)
					{
						ReadOnlyLogText subChunkDataText = new ReadOnlyLogText(lastSubChunkData);
						subChunks.Add(new LogSubChunkData((LogType)(int)type, subChunkOffset, subChunkLineIndex, subChunkDataText));
					}

					if (subChunks.Count == 0)
					{
						return null;
					}

					return new LogChunkData(offset, lineIndex, subChunks);
				}

				await transaction.WaitAndIgnoreCancellations(typeTask, lastSubChunkDataTask);

				// Cool down before retrying
				await Task.Delay(100);
			}

			_logger.LogWarning("Unable to get chunk for {LogId} at {Offset}:{LineIndex} after {NumTries} tries", logId, offset, lineIndex, numTries);
			return null;
		}

		/// <inheritdoc/>
		public async Task<List<(LogId, long)>> TouchChunksAsync(TimeSpan minAge)
		{
			IDatabase redisDb = _redisConnectionPool.GetDatabase();
			
			// Find all the chunks that are suitable for expiry
			DateTime utcNow = DateTime.UtcNow;
			SortedSetEntry[] entries = await redisDb.SortedSetRangeByScoreWithScoresAsync(ItemsKey, stop: (utcNow - minAge).Ticks);

			// Update the score for each element in a transaction. If it succeeds, we can write the chunk. Otherwise another pod has beat us to it.
			List<(LogId, long)> results = new List<(LogId, long)>();
			foreach (SortedSetEntry entry in entries)
			{
				if (ChunkKeys.TryParse(entry.Element.ToString(), out LogId logId, out long offset))
				{
					ITransaction transaction = redisDb.CreateTransaction();
					transaction.AddCondition(Condition.SortedSetEqual(ItemsKey, entry.Element, entry.Score));
					_ = transaction.SortedSetAddAsync(ItemsKey, entry.Element, utcNow.Ticks, flags: CommandFlags.FireAndForget);

					if (!await transaction.ExecuteAsync())
					{
						break;
					}

					results.Add((logId, offset));
				}
			}
			return results;
		}
	}
}
