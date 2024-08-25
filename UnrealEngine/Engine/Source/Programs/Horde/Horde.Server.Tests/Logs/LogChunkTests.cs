// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using Horde.Server.Logs;
using Horde.Server.Logs.Data;
using Microsoft.Extensions.Logging.Abstractions;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Server.Tests.Logs
{
	[TestClass]
	public class LogChunkTests
	{
		readonly string[] _text =
		{
			@"{ ""id"": 1, ""message"": ""foo"" }",
			@"{ ""id"": 2, ""message"": ""bar"" }",
			@"{ ""id"": 3, ""message"": ""baz"" }",
		};

		[TestMethod]
		public void TestSerialization()
		{
			byte[] textData = Encoding.UTF8.GetBytes(String.Join("\n", _text) + "\n");

			List<LogSubChunkData> subChunks = new List<LogSubChunkData>();
			subChunks.Add(new LogSubChunkData(LogType.Text, 0, 0, new LogText(textData, textData.Length)));
			subChunks.Add(new LogSubChunkData(LogType.Json, textData.Length, 3, new LogText(textData, textData.Length)));

			LogChunkData oldChunkData = new LogChunkData(0, 0, subChunks);
			byte[] data = oldChunkData.ToByteArray(NullLogger.Instance);
			LogChunkData newChunkData = LogChunkData.FromMemory(data, 0, 0);

			Assert.AreEqual(oldChunkData.Length, newChunkData.Length);
			Assert.AreEqual(oldChunkData.SubChunks.Count, newChunkData.SubChunks.Count);
			Assert.IsTrue(oldChunkData.SubChunkOffset.AsSpan(0, oldChunkData.SubChunkOffset.Length).SequenceEqual(newChunkData.SubChunkOffset.AsSpan(0, newChunkData.SubChunkOffset.Length)));
			Assert.IsTrue(oldChunkData.SubChunkLineIndex.AsSpan(0, oldChunkData.SubChunkLineIndex.Length).SequenceEqual(newChunkData.SubChunkLineIndex.AsSpan(0, newChunkData.SubChunkLineIndex.Length)));
			for (int idx = 0; idx < oldChunkData.SubChunks.Count; idx++)
			{
				LogSubChunkData oldSubChunkData = oldChunkData.SubChunks[idx];
				LogSubChunkData newSubChunkData = newChunkData.SubChunks[idx];
				Assert.IsTrue(oldSubChunkData.InflateText().Data.Span.SequenceEqual(newSubChunkData.InflateText().Data.Span));
			}
		}
	}
}
