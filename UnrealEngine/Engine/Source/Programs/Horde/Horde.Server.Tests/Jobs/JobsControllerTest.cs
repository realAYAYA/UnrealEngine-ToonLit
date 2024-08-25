// Copyright Epic Games, Inc. All Rights Reserved.

using System.Net;
using System.Threading.Tasks;
using Horde.Server.Jobs.Artifacts;
using Microsoft.VisualStudio.TestTools.UnitTesting;

#pragma warning disable CA2234 // Pass system uri objects instead of strings

namespace Horde.Server.Tests.Jobs
{
	[TestClass]
	public class JobsControllerTest : ControllerIntegrationTest
	{
		private static string GetUri(string jobId, string stepId, string fileName)
		{
			return $"/api/v1/jobs/{jobId}/steps/{stepId}/artifacts/{fileName}/data";
		}

		[TestMethod]
		public async Task GetArtifactDataByFilenameTestAsync()
		{
			Fixture? fixture = await GetFixtureAsync();
			IArtifactV1? art = fixture.Job1Artifact;

			// Test existing filename
			System.Net.Http.HttpResponseMessage? res = await Client.GetAsync(GetUri(art.JobId.ToString(), art.StepId.ToString()!, art.Name));
			res.EnsureSuccessStatusCode();
			Assert.AreEqual(fixture.Job1ArtifactData, await res.Content.ReadAsStringAsync());

			// Test non-existing filename
			res = await Client.GetAsync(GetUri(art.JobId.ToString(), art.StepId.ToString()!, "bogus.txt"));
			Assert.AreEqual(res.StatusCode, HttpStatusCode.NotFound);
		}
	}
}