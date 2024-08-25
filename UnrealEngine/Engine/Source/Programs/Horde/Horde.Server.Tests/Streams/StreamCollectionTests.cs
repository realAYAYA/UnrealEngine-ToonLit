// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using EpicGames.Horde.Jobs.Templates;
using EpicGames.Horde.Streams;
using Horde.Server.Streams;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Server.Tests.Streams
{
	[TestClass]
	public class StreamCollectionTests : TestSetup
	{
		private readonly StreamId _streamId = new("bogusStreamId");

		[TestMethod]
		public void ValidateUndefinedTemplateIdInTabs()
		{
			StreamConfig config = new()
			{
				Tabs = new()
				{
					new TabConfig { Templates = new List<TemplateId> { new ("foo") }},
					new TabConfig { Templates = new List<TemplateId> { new ("bar") }}
				},
				Templates = new() { new TemplateRefConfig { Id = new TemplateId("foo") } }
			};

			Assert.ThrowsException<InvalidStreamException>(() => Horde.Server.Streams.StreamCollection.Validate(_streamId, config));

			config.Templates.Add(new TemplateRefConfig { Id = new TemplateId("bar") });
			Horde.Server.Streams.StreamCollection.Validate(_streamId, config);
		}
	}
}
