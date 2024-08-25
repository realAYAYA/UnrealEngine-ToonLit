// Copyright Epic Games, Inc. All Rights Reserved.

using System.Text.Json;

namespace Horde.Server.Tests.Ddc.FunctionalTests
{
	public static class JsonTestUtils
	{
		public static readonly JsonSerializerOptions DefaultJsonSerializerSettings = ConfigureJsonOptions();

		private static JsonSerializerOptions ConfigureJsonOptions()
		{
			JsonSerializerOptions options = new JsonSerializerOptions();
			Startup.ConfigureJsonSerializer(options);
			return options;
		}
	}
}
