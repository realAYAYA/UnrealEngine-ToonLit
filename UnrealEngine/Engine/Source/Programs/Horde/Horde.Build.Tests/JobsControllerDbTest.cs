// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Threading.Tasks;
using Horde.Build.Jobs;
using Horde.Build.Jobs.Graphs;
using Microsoft.AspNetCore.Mvc;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Build.Tests
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
	        Assert.AreEqual(2, res.Value!.Count);
	        Assert.AreEqual("hello2", (res.Value[0] as GetJobResponse)!.Name);
	        Assert.AreEqual("hello1", (res.Value[1] as GetJobResponse)!.Name);
	        
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
	        object obj = (await JobsController.FindJobTimingsAsync(fixture.Stream!.Id.ToString(), templates)).Value!;
	        FindJobTimingsResponse res = (obj as FindJobTimingsResponse)!;
	        Assert.AreEqual(1, res.Timings.Count);
	        GetJobTimingResponse timingResponse = res.Timings[job.Id.ToString()];
	        Assert.AreEqual(0, timingResponse.JobResponse!.Labels!.Count);
//	        Assert.AreEqual(job.Name, timingResponse.Job!.Name);
        }
    }
}