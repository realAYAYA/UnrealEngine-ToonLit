// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Net;
using System.Threading.Tasks;
using Horde.Build.Jobs.Artifacts;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Build.Tests
{
	[TestClass]
    public class JobsControllerTest : ControllerIntegrationTest
    {
        private static Uri GetUri(string jobId, string stepId, string fileName)
        {
            return new Uri($"http://localhost/api/v1/jobs/{jobId}/steps/{stepId}/artifacts/{fileName}/data");
        }

        [TestMethod]
        public async Task GetArtifactDataByFilenameTest()
        {
			Fixture? fixture = await GetFixture();
			IArtifact? art = fixture.Job1Artifact;

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