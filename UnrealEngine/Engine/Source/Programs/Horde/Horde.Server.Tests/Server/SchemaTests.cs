// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using Horde.Server.Streams;
using Horde.Server.Utilities;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Server.Tests.Server
{
	[TestClass]
	public class SchemaTests
	{
		[TestMethod]
		public void StreamSchema()
		{
			JsonSchema schema = Schemas.CreateSchema(typeof(StreamConfig));
			_ = schema;
		}

		[TestMethod]
		public void ProjectSchema()
		{
			JsonSchema schema = Schemas.CreateSchema(typeof(StreamConfig));
			_ = schema;
		}
	}
}
