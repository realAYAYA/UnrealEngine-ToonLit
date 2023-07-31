// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Horde.Build.Compute.Tests.Properties;
using Horde.Build.Logs;
using Horde.Build.Logs.Builder;
using Horde.Build.Logs.Storage;
using Horde.Build.Jobs;
using Horde.Build.Utilities;
using HordeCommon;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Horde.Build.Logs.Data;

namespace Horde.Build.Tests
{
	using JobId = ObjectId<IJob>;

	[TestClass]
	public class LogIndexingTests : DatabaseIntegrationTest
	{
		private readonly ILogFileService _logFileService;
		private readonly NullLogStorage _nullLogStorage;
		private readonly LocalLogStorage _logStorage;
		private readonly byte[] _data = Resources.TextFile;

		public LogIndexingTests()
		{
			LogFileCollection logFileCollection = new LogFileCollection(GetMongoServiceSingleton());

			ServiceProvider serviceProvider = new ServiceCollection()
				.AddLogging(builder => builder.AddConsole().SetMinimumLevel(LogLevel.Debug))
				.BuildServiceProvider();

			ILoggerFactory loggerFactory = serviceProvider.GetRequiredService<ILoggerFactory>();
			ILogger<LogFileService> logger = loggerFactory.CreateLogger<LogFileService>();

			// Just to satisfy the parameter, need to be fixed
			IConfiguration config = new ConfigurationBuilder().Build();

			ILogBuilder logBuilder = new LocalLogBuilder();
			_nullLogStorage = new NullLogStorage();
			_logStorage = new LocalLogStorage(20, _nullLogStorage);
			_logFileService = new LogFileService(logFileCollection, null!, logBuilder, _logStorage, new FakeClock(), logger);
		}

		protected override void Dispose(bool disposing)
		{
			base.Dispose(disposing);

			_logStorage.Dispose();
			_nullLogStorage.Dispose();
		}

		[TestMethod]
		public async Task IndexTests()
		{
			JobId jobId = JobId.GenerateNewId();
			ILogFile logFile = await _logFileService.CreateLogFileAsync(jobId, null, LogType.Text);

			// Write the test data to the log file in blocks
			int offset = 0;
			int lineIndex = 0;
			while (offset < _data.Length)
			{
				int length = 0;
				int lineCount = 0;

				for(int idx = offset; idx < Math.Min(_data.Length, offset + 1883); idx++)
				{
					if(_data[idx] == '\n')
					{
						length = (idx + 1) - offset;
						lineCount++;
						break;
					}
				}

				logFile = await WriteLogDataAsync(logFile, offset, lineIndex, _data.AsMemory(offset, length), false);

				offset += length;
				lineIndex += lineCount;
			}

			// Read the data back out and check it's the same
			byte[] readData = new byte[_data.Length];
			using (Stream stream = await _logFileService.OpenRawStreamAsync(logFile, 0, _data.Length))
			{
				int readSize = await stream.ReadAsync(readData, 0, readData.Length);
				Assert.AreEqual(readData.Length, readSize);

				int equalSize = 0;
				while(equalSize < _data.Length && _data[equalSize] == readData[equalSize])
				{
					equalSize++;
				}

				Assert.AreEqual(equalSize, readSize);
			}

			// Test some searches
			await SearchLogDataTestAsync(logFile);

			// Generate an index and test again
			logFile = await WriteLogDataAsync(logFile, offset, lineIndex, Array.Empty<byte>(), true);
			await SearchLogDataTestAsync(logFile);
		}

		[TestMethod]
		public void TrieTests()
		{
			ReadOnlyTrieBuilder builder = new ReadOnlyTrieBuilder();

			ulong[] values = { 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233, 377, 610, 987, 1597, 2584, 4181, 6765, 10946, 0xfedcba9876543210UL };
			foreach (ulong value in values)
			{
				builder.Add(value);
			}

			ReadOnlyTrie trie = builder.Build();
			Assert.IsTrue(Enumerable.SequenceEqual(trie.EnumerateRange(0, UInt64.MaxValue), values));
			Assert.IsTrue(Enumerable.SequenceEqual(trie.EnumerateRange(0, 90), values.Where(x => x <= 90)));
			Assert.IsTrue(Enumerable.SequenceEqual(trie.EnumerateRange(2, 89), values.Where(x => x >= 2 && x <= 89)));
		}

		[TestMethod]
		public async Task PartialTokenTests()
		{
			JobId jobId = JobId.GenerateNewId();
			ILogFile logFile = await _logFileService.CreateLogFileAsync(jobId, null, LogType.Text);

			string[] lines =
			{
				"abcdefghi\n",
				"jklmno123\n",
				"pqrst99uv\n",
				"wx\n"
			};

			int length = 0;
			for (int lineIdx = 0; lineIdx < lines.Length; lineIdx++)
			{
				logFile = await WriteLogDataAsync(logFile, length, lineIdx, Encoding.UTF8.GetBytes(lines[lineIdx]), true);
				length += lines[lineIdx].Length;
			}

			for (int lineIdx = 0; lineIdx < lines.Length; lineIdx++)
			{
				for (int strLen = 1; strLen < 7; strLen++)
				{
					for (int strOfs = 0; strOfs + strLen < lines[lineIdx].Length - 1; strOfs++)
					{
						string str = lines[lineIdx].Substring(strOfs, strLen);

						LogSearchStats stats = new LogSearchStats();
						List<int> results = await _logFileService.SearchLogDataAsync(logFile, str, 0, 5, stats);
						Assert.AreEqual(1, results.Count);
						Assert.AreEqual(lineIdx, results[0]);

						Assert.AreEqual(1, stats.NumScannedBlocks);
						Assert.AreEqual(3, stats.NumSkippedBlocks);
						Assert.AreEqual(0, stats.NumFalsePositiveBlocks);
					}
				}
			}
		}

		[TestMethod]
		public async Task AppendIndexTests()
		{
			JobId jobId = JobId.GenerateNewId();
			ILogFile logFile = await _logFileService.CreateLogFileAsync(jobId, null, LogType.Text);

			logFile = await WriteLogDataAsync(logFile, 0, 0, Encoding.UTF8.GetBytes("abc\n"), true);
			logFile = await WriteLogDataAsync(logFile, 4, 1, Encoding.UTF8.GetBytes("def\n"), true);
			logFile = await WriteLogDataAsync(logFile, 8, 2, Encoding.UTF8.GetBytes("ghi\n"), false);

			await ((LogFileService)_logFileService).FlushPendingWritesAsync();

			{
				LogSearchStats stats = new LogSearchStats();
				List<int> results = await _logFileService.SearchLogDataAsync(logFile, "abc", 0, 5, stats);
				Assert.AreEqual(1, results.Count);
				Assert.AreEqual(0, results[0]);

				Assert.AreEqual(2, stats.NumScannedBlocks); // abc + ghi (no index yet because it hasn't been flushed)
				Assert.AreEqual(1, stats.NumSkippedBlocks); // def
				Assert.AreEqual(0, stats.NumFalsePositiveBlocks);
			}
			{
				LogSearchStats stats = new LogSearchStats();
				List<int> results = await _logFileService.SearchLogDataAsync(logFile, "def", 0, 5, stats);
				Assert.AreEqual(1, results.Count);
				Assert.AreEqual(1, results[0]);

				Assert.AreEqual(2, stats.NumScannedBlocks); // def + ghi (no index yet because it hasn't been flushed)
				Assert.AreEqual(1, stats.NumSkippedBlocks); // abc
				Assert.AreEqual(0, stats.NumFalsePositiveBlocks);
			}
			{
				LogSearchStats stats = new LogSearchStats();
				List<int> results = await _logFileService.SearchLogDataAsync(logFile, "ghi", 0, 5, stats);
				Assert.AreEqual(1, results.Count);
				Assert.AreEqual(2, results[0]);

				Assert.AreEqual(1, stats.NumScannedBlocks); // ghi
				Assert.AreEqual(2, stats.NumSkippedBlocks); // abc + def
				Assert.AreEqual(0, stats.NumFalsePositiveBlocks);
			}
		}

		async Task<ILogFile> WriteLogDataAsync(ILogFile logFile, long offset, int lineIndex, ReadOnlyMemory<byte> data, bool flush)
		{
			const int MaxChunkLength = 32 * 1024;
			const int MaxSubChunkLineCount = 128;

			ILogFile? newLogFile = await _logFileService.WriteLogDataAsync(logFile, offset, lineIndex, data, flush, MaxChunkLength, MaxSubChunkLineCount);
			Assert.IsNotNull(newLogFile);
			return newLogFile!;
		}

		async Task SearchLogDataTestAsync(ILogFile logFile)
		{
			await SearchLogDataTestAsync(logFile, "HISPANIOLA", 0, 4, new[] { 1503, 1520, 1525, 1595 });
			await SearchLogDataTestAsync(logFile, "Hispaniola", 0, 4, new[] { 1503, 1520, 1525, 1595 });
			await SearchLogDataTestAsync(logFile, "HizpaniolZ", 0, 4, Array.Empty<int>());
			await SearchLogDataTestAsync(logFile, "Pieces of eight!", 0, 100, new[] { 2227, 2228, 5840, 5841, 7520 });
			await SearchLogDataTestAsync(logFile, "NEWSLETTER", 0, 100, new[] { 7886 });
		}

		async Task SearchLogDataTestAsync(ILogFile logFile, string text, int firstLine, int count, int[] expectedLines)
		{
			LogSearchStats stats = new LogSearchStats();
			List<int> lines = await _logFileService.SearchLogDataAsync(logFile, text, firstLine, count, stats);
			Assert.IsTrue(lines.SequenceEqual(expectedLines));
		}
	}
}
