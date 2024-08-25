// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers;
using System.Text;
using System.Text.Json;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace EpicGames.Core.Tests
{
	[TestClass]
	public class LogValueTests
	{
		[LogValueType]
		class TaggedType
		{
			public override string ToString() => "hello";
		}

		static string RenderLogValue(object value)
		{
			ArrayBufferWriter<byte> buffer = new ArrayBufferWriter<byte>();
			using (Utf8JsonWriter writer = new Utf8JsonWriter(buffer))
			{
				LogValueFormatter.Format(value, writer);
			}
			return Encoding.UTF8.GetString(buffer.WrittenSpan);
		}

		[TestMethod]
		public void TaggedTypeTest()
		{
			string value = RenderLogValue(new TaggedType());
			Assert.AreEqual(value, "{\"$type\":\"TaggedType\",\"$text\":\"hello\"}");
		}

		[TestMethod]
		public void LinkTest()
		{
			string value = RenderLogValue(LogValue.Link(new Uri("https://epicgames.com/"), "Epic Games"));
			Assert.AreEqual(value, "{\"$type\":\"Link\",\"$text\":\"Epic Games\",\"target\":\"https://epicgames.com/\"}");
		}

		[TestMethod]
		public void SourceFile()
		{
			FileReference file = new FileReference("foo.txt");
			string value = RenderLogValue(LogValue.SourceFile(file, "foo"));
			Assert.AreEqual(value, "{\"$type\":\"SourceFile\",\"$text\":\"foo\",\"file\":\"" + file.FullName.Replace("\\", "\\\\", StringComparison.Ordinal) + "\"}");
		}
	}
}
