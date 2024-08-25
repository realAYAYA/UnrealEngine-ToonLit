// Copyright Epic Games, Inc. All Rights Reserved.

using System.Text.Json;
using Jupiter.Implementation;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Jupiter.Tests.Unit
{
	[TestClass]
	public class BlobIdentifierTests
	{
		[TestMethod]
		public void TestDeserialization()
		{
			string s =
				@"{""namespace"":""ue.ddc"",""bucket"":""texture"",""key"":""e5d70ae6567ffa9b5b3e69b2f45280e04fc5227f"",""op"":0,""timestamp"":""2022-01-03T16:00:00Z"",""timeBucket"":""rep-132856992000000000"",""eventId"":""546d4d89-6cae-11ec-8461-c64372ba79e9"",""blob"":null}";

			ReplicationLogEvent? e = JsonSerializer.Deserialize<ReplicationLogEvent>(s, ConfigureJsonOptions());

			Assert.IsNotNull(e);
			Assert.AreEqual("ue.ddc", e.Namespace.ToString());
			Assert.AreEqual("texture", e.Bucket.ToString());
			Assert.AreEqual("e5d70ae6567ffa9b5b3e69b2f45280e04fc5227f", e.Key.ToString());
			Assert.AreEqual(null, e.Blob);
		}

		private static JsonSerializerOptions ConfigureJsonOptions()
		{
			JsonSerializerOptions options = new JsonSerializerOptions();
			BaseStartup.ConfigureJsonOptions(options);
			return options;
		}
	}
}
