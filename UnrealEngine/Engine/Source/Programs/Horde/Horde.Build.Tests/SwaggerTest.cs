// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Net.Http;
using System.Text.Json;
using System.Threading.Tasks;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Build.Tests
{
	[TestClass]
    public class SwaggerTest : ControllerIntegrationTest
    {
        [TestMethod]
        [Ignore("Until clashes with Horde.Build and Horde.Storage are resolved")]
        public async Task ValidateSwagger()
        {
	        HttpResponseMessage res = await Client.GetAsync(new Uri("/swagger/v1/swagger.json"));
	        if (!res.IsSuccessStatusCode)
	        {
		        string rawJson = await res.Content.ReadAsStringAsync();
		        JsonElement tempElement = JsonSerializer.Deserialize<JsonElement>(rawJson);
		        string formattedJson = JsonSerializer.Serialize(tempElement, new JsonSerializerOptions { WriteIndented = true });
		        await Console.Error.WriteLineAsync("Error result:\n" + formattedJson);
		        res.EnsureSuccessStatusCode();
	        }
        }
    }
}