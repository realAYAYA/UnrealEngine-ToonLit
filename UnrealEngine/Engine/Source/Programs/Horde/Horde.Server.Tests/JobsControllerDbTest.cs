// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Threading.Tasks;
using EpicGames.Core;
using Horde.Server.Jobs;
using Horde.Server.Jobs.Graphs;
using Microsoft.AspNetCore.Mvc;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Server.Tests
{
	/// <summary>
	/// Database-only test for testing the Job controller. Different from the JobsController test that set up
	/// the entire ASP.NET chain.
	/// </summary>
	[TestClass]
    public class JobsControllerDbTest : TestSetup
    {
        
        [TestMethod]
        public async Task GetJobs()
        {
			await CreateFixtureAsync();

			ActionResult<List<object>> res = await JobsController.FindJobsAsync();

			List<GetJobResponse> responses = res.Value!.ConvertAll(x => (GetJobResponse)x);
			responses.SortBy(x => x.Change);

			Assert.AreEqual(2, responses.Count);
			Assert.AreEqual("hello1", responses[0].Name);
			Assert.AreEqual("hello2", responses[1].Name);

	        res = await JobsController.FindJobsAsync(includePreflight: false);
	        Assert.AreEqual(1, res.Value!.Count);
	        Assert.AreEqual("hello2", (res.Value[0] as GetJobResponse)!.Name);
        }
        
        [TestMethod]
        public async Task AbortStepTest()
        {
			Fixture fixture = await CreateFixtureAsync();

	        IJob job = fixture.Job1;
	        SubResourceId batchId = job.Batches[0].Id;
	        SubResourceId stepId = job.Batches[0].Steps[0].Id;

	        object obj = (await JobsController.GetStepAsync(job.Id, batchId, stepId)).Value!;
	        GetStepResponse stepRes = (obj as GetStepResponse)!;
	        Assert.IsFalse(stepRes.AbortRequested);
	        
	        UpdateStepRequest updateReq = new UpdateStepRequest();
	        updateReq.AbortRequested = true;
			await JobsController.UpdateStepAsync(job.Id, batchId, stepId, updateReq);
//	        UpdateStepResponse updateRes = (obj as UpdateStepResponse)!;
	        
	        obj = (await JobsController.GetStepAsync(job.Id, batchId, stepId)).Value!;
	        stepRes = (obj as GetStepResponse)!;
	        Assert.IsTrue(stepRes.AbortRequested);
//	        Assert.AreEqual("Anonymous", StepRes.AbortByUser);
        }
        
        [TestMethod]
        public async Task FindJobTimingsTest()
        {
	        Fixture fixture = await CreateFixtureAsync();
	        IJob job = fixture.Job1;
	        string[] templates = { job.TemplateId.ToString() };
	        object obj = (await JobsController.FindJobTimingsAsync(fixture.StreamConfig!.Id.ToString(), templates)).Value!;
	        FindJobTimingsResponse res = (obj as FindJobTimingsResponse)!;
	        Assert.AreEqual(1, res.Timings.Count);
	        GetJobTimingResponse timingResponse = res.Timings[job.Id.ToString()];
	        Assert.AreEqual(0, timingResponse.JobResponse!.Labels!.Count);
//	        Assert.AreEqual(job.Name, timingResponse.Job!.Name);
        }
    }
}