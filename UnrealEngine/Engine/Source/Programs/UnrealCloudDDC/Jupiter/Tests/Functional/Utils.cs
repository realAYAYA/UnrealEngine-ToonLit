// Copyright Epic Games, Inc. All Rights Reserved.

using System.Text.Json;

namespace Jupiter.Tests.Functional
{
	public static class JsonTestUtils
	{
		public static readonly JsonSerializerOptions DefaultJsonSerializerSettings = ConfigureJsonOptions();

		private static JsonSerializerOptions ConfigureJsonOptions()
		{
			JsonSerializerOptions options = new JsonSerializerOptions();
			BaseStartup.ConfigureJsonOptions(options);
			return options;
		}
	}
}
