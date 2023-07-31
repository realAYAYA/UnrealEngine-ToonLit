// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Threading.Tasks;
using Horde.Build.Logs;
using Horde.Build.Logs.Builder;
using Horde.Build.Logs.Data;
using Horde.Build.Server;
using Horde.Build.Utilities;
using Microsoft.Extensions.Logging.Abstractions;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Build.Tests
{
	using LogId = ObjectId<ILogFile>;

	[TestClass]
	public class LogBuilderTests : DatabaseIntegrationTest
	{
		[TestMethod]
		public async Task TestLocalLogBuilder()
		{
			ILogBuilder builder = new LocalLogBuilder();
			await TestBuilder(builder);
		}

		[TestMethod]
		public async Task TestRedisLogBuilder()
		{
			RedisService redisService = GetRedisServiceSingleton();
			ILogBuilder builder = new RedisLogBuilder(redisService.ConnectionPool, NullLogger.Instance);
			await TestBuilder(builder);
		}

		public static async Task TestBuilder(ILogBuilder builder)
		{
			LogId logId = LogId.GenerateNewId();

			const long Offset = 100;
			Assert.IsTrue(await builder.AppendAsync(logId, Offset, Offset, 0, 1, Encoding.UTF8.GetBytes("hello\n"), LogType.Text));

			LogChunkData? chunk1 = await builder.GetChunkAsync(logId, Offset, 0);
			Assert.IsNotNull(chunk1);
			Assert.AreEqual(1, chunk1!.LineCount);
			Assert.AreEqual(6, chunk1!.Length);
			Assert.AreEqual(1, chunk1!.SubChunks.Count);
			Assert.AreEqual(1, chunk1!.SubChunks[0].LineCount);
			Assert.AreEqual(6, chunk1!.SubChunks[0].Length);

			Assert.IsTrue(await builder.AppendAsync(logId, Offset, Offset + 6, 1, 1, Encoding.UTF8.GetBytes("world\n"), LogType.Text));

			LogChunkData? chunk2 = await builder.GetChunkAsync(logId, Offset, 1);
			Assert.IsNotNull(chunk2);
			Assert.AreEqual(2, chunk2!.LineCount);
			Assert.AreEqual(12, chunk2!.Length);
			Assert.AreEqual(1, chunk2!.SubChunks.Count);
			Assert.AreEqual(2, chunk2!.SubChunks[0].LineCount);
			Assert.AreEqual(12, chunk2!.SubChunks[0].Length);

			await builder.CompleteSubChunkAsync(logId, Offset);
			Assert.IsTrue(await builder.AppendAsync(logId, Offset, Offset + 12, 2, 1, Encoding.UTF8.GetBytes("foo\n"), LogType.Text));
			Assert.IsTrue(await builder.AppendAsync(logId, Offset, Offset + 16, 3, 2, Encoding.UTF8.GetBytes("bar\nbaz\n"), LogType.Text));
			await builder.CompleteSubChunkAsync(logId, Offset);

			LogChunkData? chunk3 = await builder.GetChunkAsync(logId, Offset, 4);
			Assert.IsNotNull(chunk3);
			Assert.AreEqual(5, chunk3!.LineCount);
			Assert.AreEqual(24, chunk3!.Length);
			Assert.AreEqual(2, chunk3!.SubChunks.Count);
			Assert.AreEqual(2, chunk3!.SubChunks[0].LineCount);
			Assert.AreEqual(12, chunk3!.SubChunks[0].Length);
			Assert.AreEqual(3, chunk3!.SubChunks[1].LineCount);
			Assert.AreEqual(12, chunk3!.SubChunks[1].Length);

			Assert.AreEqual(0, chunk3!.GetLineOffsetWithinChunk(0));
			Assert.AreEqual(6, chunk3!.GetLineOffsetWithinChunk(1));
			Assert.AreEqual(12, chunk3!.GetLineOffsetWithinChunk(2));
			Assert.AreEqual(16, chunk3!.GetLineOffsetWithinChunk(3));
			Assert.AreEqual(20, chunk3!.GetLineOffsetWithinChunk(4));

			List<(LogId, long)> chunks1 = await builder.TouchChunksAsync(TimeSpan.Zero);
			Assert.AreEqual(1, chunks1.Count);
			Assert.AreEqual((logId, Offset), chunks1[0]);

			await builder.CompleteChunkAsync(logId, Offset);

			LogChunkData? chunk4 = await builder.GetChunkAsync(logId, Offset, 5);
			Assert.AreEqual(24, chunk4!.Length);

			List<(LogId, long)> chunks2 = await builder.TouchChunksAsync(TimeSpan.Zero);
			Assert.AreEqual(1, chunks2.Count);

			await builder.RemoveChunkAsync(logId, Offset);

			List<(LogId, long)> chunks3 = await builder.TouchChunksAsync(TimeSpan.Zero);
			Assert.AreEqual(0, chunks3.Count);
		}
	}
}
