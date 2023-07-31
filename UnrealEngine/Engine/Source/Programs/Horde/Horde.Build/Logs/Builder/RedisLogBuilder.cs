// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Globalization;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Redis;
using Horde.Build.Logs.Data;
using Horde.Build.Utilities;
using Microsoft.Extensions.Logging;
using OpenTracing;
using OpenTracing.Util;
using StackExchange.Redis;

namespace Horde.Build.Logs.Builder
{
	using Condition = StackExchange.Redis.Condition;
	using LogId = ObjectId<ILogFile>;

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
		/// Logger for debug output
		/// </summary>
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="redisConnectionPool">The redis database singleton</param>
		/// <param name="logger">Logger for debug output</param>
		public RedisLogBuilder(RedisConnectionPool redisConnectionPool, ILogger logger)
		{
			_redisConnectionPool = redisConnectionPool;
			_logger = logger;
		}

		/// <inheritdoc/>
		public async Task<bool> AppendAsync(LogId logId, long chunkOffset, long writeOffset, int writeLineIndex, int writeLineCount, ReadOnlyMemory<byte> data, LogType type)
		{
			IDatabase redisDb = _redisConnectionPool.GetDatabase();
			ChunkKeys keys = new ChunkKeys(logId, chunkOffset);

			if(chunkOffset == writeOffset)
			{
				using IScope scope = GlobalTracer.Instance.BuildSpan("Redis.CreateChunk").StartActive();
				scope.Span.SetTag("LogId", logId.ToString());
				scope.Span.SetTag("Offset", chunkOffset.ToString(CultureInfo.InvariantCulture));
				scope.Span.SetTag("WriteOffset", writeOffset.ToString(CultureInfo.InvariantCulture));

				ITransaction? createTransaction = redisDb.CreateTransaction();
				createTransaction.AddCondition(Condition.SortedSetNotContains(ItemsKey, keys.Prefix));
				_ = createTransaction.SortedSetAddAsync(ItemsKey, keys.Prefix, DateTime.UtcNow.Ticks);
				_ = createTransaction.StringSetAsync(keys.Type, (int)type);
				_ = createTransaction.StringSetAsync(keys.Length, data.Length);
				_ = createTransaction.StringSetAsync(keys.LineIndex, writeLineIndex + writeLineCount);
				_ = createTransaction.StringSetAsync(keys.SubChunkData, data);
				if (await createTransaction.ExecuteAsync())
				{
					_logger.LogTrace("Created {Size} bytes in {Key}", data.Length, keys.SubChunkData);
					return true;
				}
			}

			{
				using IScope scope = GlobalTracer.Instance.BuildSpan("Redis.AppendChunk").StartActive();
				scope.Span.SetTag("LogId", logId.ToString());
				scope.Span.SetTag("Offset", chunkOffset.ToString(CultureInfo.InvariantCulture));
				scope.Span.SetTag("WriteOffset", writeOffset.ToString(CultureInfo.InvariantCulture));

				ITransaction? appendTransaction = redisDb.CreateTransaction();
				appendTransaction.AddCondition(Condition.SortedSetContains(ItemsKey, keys.Prefix));
				appendTransaction.AddCondition(Condition.KeyNotExists(keys.Complete));
				appendTransaction.AddCondition(Condition.StringEqual(keys.Type, (int)type));
				appendTransaction.AddCondition(Condition.StringEqual(keys.Length, (int)(writeOffset - chunkOffset)));
				_ = appendTransaction.StringAppendAsync(keys.SubChunkData, data);
				_ = appendTransaction.StringSetAsync(keys.Length, (int)(writeOffset - chunkOffset) + data.Length);
				_ = appendTransaction.StringSetAsync(keys.LineIndex, writeLineIndex + writeLineCount);
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
			for (; ; )
			{
				using IScope scope = GlobalTracer.Instance.BuildSpan("Redis.CompleteSubChunk").StartActive();
				scope.Span.SetTag("LogId", logId.ToString());
				scope.Span.SetTag("Offset", offset.ToString(CultureInfo.InvariantCulture));

				LogType type = (LogType)(int)await redisDb.StringGetAsync(keys.Type);
				int length = (int)await redisDb.StringGetAsync(keys.Length);
				int lineIndex = (int)await redisDb.StringGetAsync(keys.LineIndex);
				long chunkDataLength = await redisDb.StringLengthAsync(keys.ChunkData);

				RedisValue subChunkTextValue = await redisDb.StringGetAsync(keys.SubChunkData);
				if (subChunkTextValue.IsNullOrEmpty)
				{
					break;
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
				_ = writeTransaction.KeyDeleteAsync(keys.SubChunkData);

				if (await writeTransaction.ExecuteAsync())
				{
					_logger.LogDebug("Completed sub-chunk for log {LogId} chunk offset {Offset} -> sub-chunk size {SubChunkSize}, chunk size {ChunkSize}", logId, offset, subChunkDataBytes.Length, await newLength);
					break;
				}
			}
		}

		/// <inheritdoc/>
		public async Task CompleteChunkAsync(LogId logId, long offset)
		{
			IDatabase redisDb = _redisConnectionPool.GetDatabase();
			using IScope scope = GlobalTracer.Instance.BuildSpan("Redis.CompleteChunk").StartActive();
			scope.Span.SetTag("LogId", logId.ToString());
			scope.Span.SetTag("Offset", offset.ToString(CultureInfo.InvariantCulture));

			ChunkKeys keys = new ChunkKeys(logId, offset);

			ITransaction transaction = redisDb.CreateTransaction();
			transaction.AddCondition(Condition.SortedSetContains(ItemsKey, keys.Prefix));
			_ = transaction.StringSetAsync(keys.Complete, true);
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
			using IScope scope = GlobalTracer.Instance.BuildSpan("Redis.RemoveChunk").StartActive();
			scope.Span.SetTag("LogId", logId.ToString());
			scope.Span.SetTag("Offset", offset.ToString(CultureInfo.InvariantCulture));

			ChunkKeys keys = new ChunkKeys(logId, offset);

			ITransaction transaction = redisDb.CreateTransaction();
			_ = transaction.KeyDeleteAsync(keys.Type);
			_ = transaction.KeyDeleteAsync(keys.LineIndex);
			_ = transaction.KeyDeleteAsync(keys.Length);
			_ = transaction.KeyDeleteAsync(keys.ChunkData);
			_ = transaction.KeyDeleteAsync(keys.SubChunkData);
			_ = transaction.KeyDeleteAsync(keys.Complete);

			_ = transaction.SortedSetRemoveAsync(ItemsKey, keys.Prefix);
			await transaction.ExecuteAsync();
		}

		/// <inheritdoc/>
		public async Task<LogChunkData?> GetChunkAsync(LogId logId, long offset, int lineIndex)
		{
			IDatabase redisDb = _redisConnectionPool.GetDatabase();
			ChunkKeys keys = new ChunkKeys(logId, offset);
			for (; ; )
			{
				using IScope scope = GlobalTracer.Instance.BuildSpan("Redis.GetChunk").StartActive();
				scope.Span.SetTag("LogId", logId.ToString());
				scope.Span.SetTag("Offset", offset.ToString(CultureInfo.InvariantCulture));

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
					while (reader.Memory.Length > 0)
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
						break;
					}

					return new LogChunkData(offset, lineIndex, subChunks);
				}
			}
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
					_ = transaction.SortedSetAddAsync(ItemsKey, entry.Element, utcNow.Ticks);

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
