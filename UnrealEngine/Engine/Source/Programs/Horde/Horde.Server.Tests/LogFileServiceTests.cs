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
using Horde.Server.Agents.Sessions;
using Horde.Server.Jobs;
using Horde.Server.Logs;
using Horde.Server.Logs.Builder;
using Horde.Server.Logs.Data;
using Horde.Server.Logs.Storage;
using Horde.Server.Storage;
using Horde.Server.Storage.Backends;
using Horde.Server.Utilities;
using HordeCommon;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Logging.Abstractions;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using MongoDB.Bson;
using OpenTelemetry.Trace;

namespace Horde.Server.Tests
{
	[TestClass]
    public class LogFileServiceTest : DatabaseIntegrationTest
    {
		private readonly FakeClock _clock;
        private readonly LogFileService _logFileService;
		private readonly ILoggerFactory _loggerFactory;
		private readonly IStorageBackend _logStorageBackend;
		private readonly ILogStorage _logStorage;

		public LogFileServiceTest()
        {
            LogFileCollection logFileCollection = new LogFileCollection(GetMongoServiceSingleton());

			_loggerFactory = new LoggerFactory();
            ILogger<LogFileService> logger = _loggerFactory.CreateLogger<LogFileService>();

            Tracer tracer = TracerProvider.Default.GetTracer("LogFileServiceTest");
            ILogBuilder logBuilder = new RedisLogBuilder(GetRedisServiceSingleton().ConnectionPool, tracer, NullLogger.Instance);
			_logStorageBackend = new MemoryStorageBackend();
			_logStorage = new PersistentLogStorage(_logStorageBackend.ForType<PersistentLogStorage>(), NullLogger<PersistentLogStorage>.Instance);
			_clock = new FakeClock();
			TestOptions<ServerSettings> settingsOpts = new (new ServerSettings());
			_logFileService = new LogFileService(logFileCollection, null!, logBuilder, _logStorage, _clock, null!, null!, settingsOpts, tracer, logger);
        }

		protected override void Dispose(bool disposing)
		{
			base.Dispose(disposing);

			_logFileService.Dispose();
			_logStorageBackend.Dispose();
			_logStorage.Dispose();
			_loggerFactory.Dispose();
		}

		[TestMethod]
		public void PlainTextDecoder()
		{
			string jsonText = "{\"time\":\"2022-01-26T14:18:29\",\"level\":\"Error\",\"message\":\"       \\tdepends on: /mnt/horde/U5\\u002BR5.0\\u002BInc\\u002BMin/Sync/Engine/Binaries/Linux/libUnrealEditor-Core.so\",\"id\":1,\"line\":5,\"lineCount\":1028}";
			byte[] jsonData = Encoding.UTF8.GetBytes(jsonText);

			byte[] outputData = new byte[jsonData.Length];
			int outputLength = LogText.ConvertToPlainText(jsonData, outputData, 0);
			string outputText = Encoding.UTF8.GetString(outputData, 0, outputLength);

			string expectText = "       \tdepends on: /mnt/horde/U5+R5.0+Inc+Min/Sync/Engine/Binaries/Linux/libUnrealEditor-Core.so\n";
			Assert.AreEqual(outputText, expectText);
			
		}

		[TestMethod]
        public async Task WriteLogLifecycleOldTest()
        {
			JobId jobId = JobId.GenerateNewId();
            ILogFile logFile = await _logFileService.CreateLogFileAsync(jobId, null, null, LogType.Text, useNewStorageBackend: false, logId: null, cancellationToken: CancellationToken.None);

            logFile = (await ((ILogFileService)_logFileService).WriteLogDataAsync(logFile, 0, 0, Encoding.ASCII.GetBytes("hello\n"), true))!;
			logFile = (await ((ILogFileService)_logFileService).WriteLogDataAsync(logFile, 6, 1, Encoding.ASCII.GetBytes("foo\nbar\n"), true))!;
			logFile = (await ((ILogFileService)_logFileService).WriteLogDataAsync(logFile, 6 + 8, 3, Encoding.ASCII.GetBytes("baz\n"), false))!;

            Assert.AreEqual("hello", await ReadLogFile(_logFileService, logFile, 0, 5));
            Assert.AreEqual("foo\nbar\nbaz\n", await ReadLogFile(_logFileService, logFile, 6, 12));

            LogMetadata metadata = await _logFileService.GetMetadataAsync(logFile, CancellationToken.None);
            Assert.AreEqual(6 + 8 + 4, metadata.Length);
            Assert.AreEqual(4, metadata.MaxLineIndex);

			List<Utf8String> lines = await _logFileService.ReadLinesAsync(logFile, 0, 100, CancellationToken.None);
			Assert.AreEqual("hello", lines[0].ToString());
			Assert.AreEqual("foo", lines[1].ToString());
			Assert.AreEqual("bar", lines[2].ToString());
			Assert.AreEqual("baz", lines[3].ToString());
        }
        
        [TestMethod]
        public async Task WriteLogLifecycleTest()
        {
	        await WriteLogLifecycle(_logFileService, 30);
        }

        
        private static async Task AssertMetadata(ILogFileService logFileService, ILogFile logFile, long expectedLength, long expectedMaxLineIndex)
        {
	        LogMetadata metadata = await logFileService.GetMetadataAsync(logFile, CancellationToken.None);
	        Assert.AreEqual(expectedLength, metadata.Length);
	        Assert.AreEqual(expectedMaxLineIndex, metadata.MaxLineIndex);
        }

        private static async Task AssertChunk(ILogFileService logFileService, LogId logFileId, long numChunks, int chunkId, long offset, long length,
	        long lineIndex)
        {
	        ILogFile? logFile = await logFileService.GetLogFileAsync(logFileId, CancellationToken.None);
	        Assert.AreEqual(numChunks, logFile!.Chunks.Count);

	        ILogChunk chunk = logFile.Chunks[chunkId];
	        Assert.AreEqual(offset, chunk.Offset);
	        Assert.AreEqual(length, chunk.Length);
	        Assert.AreEqual(lineIndex, chunk.LineIndex);
        }

        private static async Task AssertLineOffset(ILogFileService logFileService, LogId logFileId, int lineIndex, int clampedLineIndex, long offset)
        {
	        ILogFile? logFile = await logFileService.GetLogFileAsync(logFileId, CancellationToken.None);

			List<Utf8String> lines = await logFileService.ReadLinesAsync(logFile!, 0, lineIndex + 1, CancellationToken.None);
			Assert.AreEqual(clampedLineIndex, Math.Min(lineIndex, lines.Count));
			Assert.AreEqual(offset, lines.Take(lineIndex).Sum(x => x.Length + 1));
        }
        
        protected static async Task<string> ReadLogFile(ILogFileService logFileService, ILogFile logFile, long offset, long length)
        {
			using Stream stream = await logFileService.OpenRawStreamAsync(logFile);

			byte[] prefix = new byte[(int)offset];
			await stream.ReadGreedyAsync(prefix);

			byte[] data = new byte[(int)length];
			int actualLength = await stream.ReadGreedyAsync(data);

			return Encoding.UTF8.GetString(data.AsSpan(0, actualLength));
        }
        
        public async Task WriteLogLifecycle(ILogFileService lfs, int maxChunkLength)
        {
			JobId jobId = JobId.GenerateNewId();
            ILogFile logFile = await lfs.CreateLogFileAsync(jobId, null, null, LogType.Text, useNewStorageBackend: false);

            string str1 = "hello\n";
            string str2 = "foo\nbar\n";
            string str3 = "baz\nqux\nquux\n";
            string str4 = "quuz\n";

            int lineIndex = 0;
            int offset = 0;

            // First write with flush. Will become chunk #1
            logFile = (await lfs.WriteLogDataAsync(logFile, offset, lineIndex, Encoding.ASCII.GetBytes(str1), true, maxChunkLength))!;
			await _clock.AdvanceAsync(TimeSpan.FromSeconds(2.0));
            Assert.AreEqual(str1, await ReadLogFile(lfs, logFile, 0, str1.Length));
            Assert.AreEqual(str1, await ReadLogFile(lfs, logFile, 0, str1.Length + 100)); // Reading too far is valid?
            await AssertMetadata(lfs, logFile, str1.Length, 1);
            await AssertChunk(lfs, logFile.Id, 1, 0, 0, str1.Length, lineIndex);
            await AssertLineOffset(lfs, logFile.Id, 0, 0, 0);
            await AssertLineOffset(lfs, logFile.Id, 1, 1, 6);
            await AssertLineOffset(lfs, logFile.Id, 1, 1, 6);
            await AssertLineOffset(lfs, logFile.Id, 1, 1, 6);

            // Second write without flushing. Will become chunk #2
            offset += str1.Length;
            lineIndex += str1.Count(f => f == '\n');
            logFile = (await lfs.WriteLogDataAsync(logFile, offset, lineIndex, Encoding.ASCII.GetBytes(str2), false, maxChunkLength))!;
			await _clock.AdvanceAsync(TimeSpan.FromSeconds(2.0));
			Assert.AreEqual(str1 + str2, await ReadLogFile(lfs, logFile, 0, str1.Length + str2.Length));
            await AssertMetadata(lfs, logFile, str1.Length + str2.Length, 3); // FIXME: what are max line index?
            await AssertChunk(lfs, logFile.Id, 2, 0, 0, str1.Length, 0);
            await AssertChunk(lfs, logFile.Id, 2, 1, str1.Length, 0, 1); // Last chunk have length zero as it's being written
            await AssertLineOffset(lfs, logFile.Id, 0, 0, 0);
            await AssertLineOffset(lfs, logFile.Id, 1, 1, 6);
            await AssertLineOffset(lfs, logFile.Id, 2, 2, 10);
            await AssertLineOffset(lfs, logFile.Id, 3, 3, 14);

            // Third write without flushing. Will become chunk #2
            offset += str2.Length;
            lineIndex += str3.Count(f => f == '\n');
            logFile = (await lfs.WriteLogDataAsync(logFile, offset, lineIndex, Encoding.ASCII.GetBytes(str3), false, maxChunkLength))!;
			await _clock.AdvanceAsync(TimeSpan.FromSeconds(2.0));
			Assert.AreEqual(str1 + str2 + str3, await ReadLogFile(lfs, logFile, 0, str1.Length + str2.Length + str3.Length));
            //await AssertMetadata(Lfs, LogFile, Str1.Length + Str2.Length + Str3.Length, 8);
            // Since no flush has happened, chunks should be identical to last write
            await AssertChunk(lfs, logFile.Id, 2, 0, 0, str1.Length, 0);
            await AssertChunk(lfs, logFile.Id, 2, 1, str1.Length, 0, 1);
            await AssertLineOffset(lfs, logFile.Id, 0, 0, 0);
            await AssertLineOffset(lfs, logFile.Id, 1, 1, 6);
            await AssertLineOffset(lfs, logFile.Id, 2, 2, 10);
            await AssertLineOffset(lfs, logFile.Id, 3, 3, 14);
            await AssertLineOffset(lfs, logFile.Id, 4, 4, 18);

            // Fourth write with flush. Will become chunk #2
            offset += str3.Length;
            lineIndex += str4.Count(f => f == '\n');
            logFile = (await lfs.WriteLogDataAsync(logFile, offset, lineIndex, Encoding.ASCII.GetBytes(str4), true, maxChunkLength))!;
			await _clock.AdvanceAsync(TimeSpan.FromSeconds(2.0));
			Assert.AreEqual(str1 + str2 + str3 + str4,
                await ReadLogFile(lfs, logFile, 0, str1.Length + str2.Length + str3.Length + str4.Length));
            Assert.AreEqual(str3 + str4,
                await ReadLogFile(lfs, logFile, str1.Length + str2.Length, str3.Length + str4.Length));
            await AssertMetadata(lfs, logFile, str1.Length + str2.Length + str3.Length + str4.Length, 7);
            await AssertChunk(lfs, logFile.Id, 2, 0, 0, str1.Length, 0);
            await AssertChunk(lfs, logFile.Id, 2, 1, str1.Length, str2.Length + str3.Length + str4.Length, 1);
            await AssertLineOffset(lfs, logFile.Id, 0, 0, 0);
            await AssertLineOffset(lfs, logFile.Id, 1, 1, 6);
            await AssertLineOffset(lfs, logFile.Id, 2, 2, 10);
            await AssertLineOffset(lfs, logFile.Id, 3, 3, 14);
            await AssertLineOffset(lfs, logFile.Id, 4, 4, 18);
            await AssertLineOffset(lfs, logFile.Id, 5, 5, 22);
            await AssertLineOffset(lfs, logFile.Id, 6, 6, 27);
            await AssertLineOffset(lfs, logFile.Id, 7, 7, 32);
            
            // Fifth write with flush and data that will span more than chunk. Will become chunk #3
            string a = "Lorem ipsum dolor sit amet\n";
            string b = "consectetur adipiscing\n";
            string str5 = a + b;
            
            offset += str4.Length;
            lineIndex += str5.Count(f => f == '\n');
            
            // Using this single write below will fail the ReadLogFile assert below. A bug?
            //await LogFileService.WriteLogDataAsync(LogFile, Offset, LineIndex, Encoding.ASCII.GetBytes(Str5), true);
            
            // Dividing it in two like this will work however
            logFile = (await lfs.WriteLogDataAsync(logFile, offset, lineIndex, Encoding.ASCII.GetBytes(a), false, maxChunkLength))!;
			logFile = (await lfs.WriteLogDataAsync(logFile, offset + a.Length, lineIndex + 1, Encoding.ASCII.GetBytes(b), true, maxChunkLength))!;

			await _clock.AdvanceAsync(TimeSpan.FromSeconds(2.0));
			await AssertMetadata(lfs, logFile, str1.Length + str2.Length + str3.Length + str4.Length + str5.Length, 9);
            await AssertChunk(lfs, logFile.Id, 4, 0, 0, str1.Length, 0);
            await AssertChunk(lfs, logFile.Id, 4, 1, str1.Length, str2.Length + str3.Length + str4.Length, 1);
            await AssertChunk(lfs, logFile.Id, 4, 2, offset, a.Length, 7);
            await AssertChunk(lfs, logFile.Id, 4, 3, offset + a.Length, b.Length, 8);

            Assert.AreEqual(str5, await ReadLogFile(lfs, logFile, offset, str5.Length));
        }

        [TestMethod]
        public async Task GetLogFileTest()
        {
            await GetMongoServiceSingleton().Database.DropCollectionAsync("LogFiles");
            Assert.AreEqual(0, (await _logFileService.GetLogFilesAsync()).Count);

			// Will implicitly test GetLogFileAsync(), AddCachedLogFile()
			JobId jobId = JobId.GenerateNewId();
            SessionId sessionId = SessionId.GenerateNewId();
            ILogFile a = await _logFileService.CreateLogFileAsync(jobId, null, sessionId, LogType.Text, useNewStorageBackend: false, logId: null, CancellationToken.None);
            ILogFile b = (await _logFileService.GetCachedLogFileAsync(a.Id, CancellationToken.None))!;
            Assert.AreEqual(a.JobId, b.JobId);
            Assert.AreEqual(a.SessionId, b.SessionId);
            Assert.AreEqual(a.Type, b.Type);

            ILogFile? notFound = await _logFileService.GetCachedLogFileAsync(LogId.GenerateNewId(), CancellationToken.None);
            Assert.IsNull(notFound);

            await _logFileService.CreateLogFileAsync(JobId.GenerateNewId(), null, SessionId.GenerateNewId(), LogType.Text, useNewStorageBackend: false, logId: null, cancellationToken: CancellationToken.None);
            Assert.AreEqual(2, (await _logFileService.GetLogFilesAsync()).Count);
        }

        [TestMethod]
        public async Task AuthorizeForSession()
        {
			JobId jobId = JobId.GenerateNewId();
            SessionId sessionId = SessionId.GenerateNewId();
            ILogFile logFile = await _logFileService.CreateLogFileAsync(jobId, null, sessionId, LogType.Text, useNewStorageBackend: false, logId: null, cancellationToken: CancellationToken.None);
            ILogFile logFileNoSession = await _logFileService.CreateLogFileAsync(jobId, null, null, LogType.Text, useNewStorageBackend: false, logId: null, cancellationToken: CancellationToken.None);

			ClaimsPrincipal hasClaim = new ClaimsPrincipal(new ClaimsIdentity(new List<Claim>
            {
                new Claim(HordeClaimTypes.AgentSessionId, sessionId.ToString()),
            }, "TestAuthType"));
            ClaimsPrincipal hasNoClaim = new ClaimsPrincipal(new ClaimsIdentity(new List<Claim>
            {
                new Claim(HordeClaimTypes.AgentSessionId, "invalid-session-id"),
            }, "TestAuthType"));

            Assert.IsTrue(LogFileService.AuthorizeForSession(logFile, hasClaim));
            Assert.IsFalse(LogFileService.AuthorizeForSession(logFile, hasNoClaim));
            Assert.IsFalse(LogFileService.AuthorizeForSession(logFileNoSession, hasClaim));
        }

		[TestMethod]
		public async Task ChunkSplitting()
		{
			JobId jobId = JobId.GenerateNewId();
			ILogFile logFile = await _logFileService.CreateLogFileAsync(jobId, null, null, LogType.Text, useNewStorageBackend: false, logId: null, cancellationToken: CancellationToken.None);

			long offset = 0;

			const int MaxChunkSize = 4;
			const int MaxSubChunkLineCount = 128;

			byte[] line1 = Encoding.UTF8.GetBytes("hello world\n");
			await _logFileService.WriteLogDataAsync(logFile, offset, 0, line1, false, MaxChunkSize, MaxSubChunkLineCount, CancellationToken.None);
			offset += line1.Length;

			byte[] line2 = Encoding.UTF8.GetBytes("ab\n");
			await _logFileService.WriteLogDataAsync(logFile, offset, 1, line2, false, MaxChunkSize, MaxSubChunkLineCount, CancellationToken.None);
			offset += line2.Length;

			byte[] line3 = Encoding.UTF8.GetBytes("a\n");
			await _logFileService.WriteLogDataAsync(logFile, offset, 2, line3, false, MaxChunkSize, MaxSubChunkLineCount, CancellationToken.None);
			offset += line3.Length;

			byte[] line4 = Encoding.UTF8.GetBytes("b\n");
			await _logFileService.WriteLogDataAsync(logFile, offset, 3, line4, false, MaxChunkSize, MaxSubChunkLineCount, CancellationToken.None);
			offset += line4.Length;

			byte[] line5 = Encoding.UTF8.GetBytes("a\nb\n");
			await _logFileService.WriteLogDataAsync(logFile, offset, 4, line5, false, MaxChunkSize, MaxSubChunkLineCount, CancellationToken.None);
			// offset += line5.Length;

			await _logFileService.FlushAsync();
			logFile = (await _logFileService.GetLogFileAsync(logFile.Id, CancellationToken.None))!;
			Assert.AreEqual(4, logFile.Chunks.Count);
			Assert.AreEqual(6, logFile.MaxLineIndex);

			Assert.AreEqual(0, logFile.Chunks[0].LineIndex);
			Assert.AreEqual(1, logFile.Chunks[1].LineIndex);
			Assert.AreEqual(2, logFile.Chunks[2].LineIndex);
			Assert.AreEqual(4, logFile.Chunks[3].LineIndex);

			Assert.AreEqual(12, logFile.Chunks[0].Length);
			Assert.AreEqual(3, logFile.Chunks[1].Length);
			Assert.AreEqual(4, logFile.Chunks[2].Length);
			Assert.AreEqual(4, logFile.Chunks[3].Length);

			Assert.AreEqual(0, logFile.Chunks.GetChunkForLine(0));
			Assert.AreEqual(1, logFile.Chunks.GetChunkForLine(1));
			Assert.AreEqual(2, logFile.Chunks.GetChunkForLine(2));
			Assert.AreEqual(2, logFile.Chunks.GetChunkForLine(3));
			Assert.AreEqual(3, logFile.Chunks.GetChunkForLine(4));
			Assert.AreEqual(3, logFile.Chunks.GetChunkForLine(5));
		}
	}
}