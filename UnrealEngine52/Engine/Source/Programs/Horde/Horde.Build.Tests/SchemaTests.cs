// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using Horde.Build.Streams;
using Horde.Build.Utilities;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Build.Tests
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
