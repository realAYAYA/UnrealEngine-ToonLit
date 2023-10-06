// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using EpicGames.Tracing.UnrealInsights;
using EpicGames.Tracing.UnrealInsights.Events;

namespace EpicGames.Tracing.Tests.UnrealInsights
{
	[TestClass]
	public class EventTest
	{
		[TestMethod]
		public void EnterScopeEventTimestampDeserialize()
		{
			{
				using MemoryStream Ms = new MemoryStream(new byte[] {0x10, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
				using BinaryReader Reader = new BinaryReader(Ms);
				EnterScopeEventTimestamp Event = EnterScopeEventTimestamp.Deserialize(Reader);
				Assert.AreEqual((ulong)0x05, Event.Timestamp);
			}

			{
				using MemoryStream Ms = new MemoryStream(new byte[] {0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF});
				using BinaryReader Reader = new BinaryReader(Ms);
				EnterScopeEventTimestamp Event = EnterScopeEventTimestamp.Deserialize(Reader);
				Assert.AreEqual((ulong)0x00_FF_00_00_00_00_00_00, Event.Timestamp);
			}
		}
	}
}