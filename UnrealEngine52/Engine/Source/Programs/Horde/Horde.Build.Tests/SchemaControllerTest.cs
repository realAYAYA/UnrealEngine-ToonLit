// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Net;
using System.Net.Http;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Threading.Tasks;
using Horde.Build.Jobs.Artifacts;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Build.Tests
{
	[TestClass]
	public class SchemaControllerTest : ControllerIntegrationTest
	{
		class SchemaCatalog
		{
			public List<SchemaCatalogItem> Schemas { get; set; } = new List<SchemaCatalogItem>();
		}

		class SchemaCatalogItem
		{
			public string? Name { get; set; }
			public Uri Url { get; set; } = null!;
		}

		[TestMethod]
		public async Task TestCatalogAsync()
		{
			await GetFixture();

			// Fetch the catalog
			SchemaCatalog catalog;
			using (HttpResponseMessage response = await Client.GetAsync(new Uri("http://localhost/api/v1/schema/catalog.json")))
			{
				response.EnsureSuccessStatusCode();
				byte[] data = await response.Content.ReadAsByteArrayAsync();
				catalog = JsonSerializer.Deserialize<SchemaCatalog>(data, new JsonSerializerOptions { PropertyNameCaseInsensitive = true })!;
			}

			// Fetch all the individual files
			foreach (SchemaCatalogItem schema in catalog.Schemas)
			{
				using HttpResponseMessage response = await Client.GetAsync(schema.Url);
				response.EnsureSuccessStatusCode();
			}
		}
	}
}
