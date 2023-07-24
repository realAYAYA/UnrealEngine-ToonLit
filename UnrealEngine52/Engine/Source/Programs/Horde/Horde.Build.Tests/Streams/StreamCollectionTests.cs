// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using Horde.Build.Streams;
using Horde.Build.Utilities;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Build.Tests.Streams
{
	using StreamId = StringId<IStream>;
	using TemplateId = StringId<ITemplateRef>;

	[TestClass]
	public class StreamCollectionTests : TestSetup
	{
		private readonly StreamId _streamId = new ("bogusStreamId");
		
		[TestMethod]
		public void ValidateUndefinedTemplateIdInTabs()
		{
			StreamConfig config = new()
			{
				Tabs = new()
				{
					new JobsTabConfig { Templates = new List<TemplateId> { new ("foo") }},
					new JobsTabConfig { Templates = new List<TemplateId> { new ("bar") }}
				},
				Templates = new () { new TemplateRefConfig { Id = new TemplateId("foo") } }
			};

			Assert.ThrowsException<InvalidStreamException>(() => Horde.Build.Streams.StreamCollection.Validate(_streamId, config));

			config.Templates.Add(new TemplateRefConfig { Id = new TemplateId("bar") });
			Horde.Build.Streams.StreamCollection.Validate(_streamId, config);
		}
	}
}
