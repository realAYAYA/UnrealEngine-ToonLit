// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Threading.Tasks;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Horde.Server.Dashboard;
using System.Linq;

namespace Horde.Server.Tests
{
	/// <summary>
	/// Tests for the dashboard
	/// </summary>
	[TestClass]
	public class DashboardTest : TestSetup
	{
		[TestMethod]
		public async Task TestPreviews()
		{
			CreateDashboardPreviewRequest request = new CreateDashboardPreviewRequest();
			request.Summary = "This is a test preview item";

			// create a reservation
			GetDashboardPreviewResponse result =  (await DashboardController!.CreateDashbordPreview(request)).Value!;

			Assert.AreEqual(1, result.Id);
			Assert.AreEqual("This is a test preview item", result.Summary);

			request.Summary = "This is another test preview item";
			result = (await DashboardController!.CreateDashbordPreview(request)).Value!;

			Assert.AreEqual(2, result.Id);
			Assert.AreEqual("This is another test preview item", result.Summary);

			UpdateDashboardPreviewRequest updateRequest = new UpdateDashboardPreviewRequest();
			updateRequest.Id = 2;
			updateRequest.Summary = "Updated test summary";
			updateRequest.ExampleLink = "http://testexample";
			updateRequest.TrackingLink = "http://testtracking";
			updateRequest.DiscussionLink = "http://testdiscussion";
			updateRequest.DeployedCL = 123;

			result = (await DashboardController!.UpdateDashbordPreview(updateRequest)).Value!;

			Assert.AreEqual(2, result.Id);
			Assert.AreEqual("http://testexample", result.ExampleLink);
			Assert.AreEqual("http://testtracking", result.TrackingLink);
			Assert.AreEqual("http://testdiscussion", result.DiscussionLink);
			Assert.AreEqual(123, result.DeployedCL);

			List<GetDashboardPreviewResponse> results = (await DashboardController!.GetDashbordPreviews(true)).Value!;
			results = results.OrderBy(r => r.Id).ToList();
			Assert.AreEqual(2, results.Count);
			Assert.AreEqual(1, results[0].Id);
			Assert.AreEqual(2, results[1].Id);

			UpdateDashboardPreviewRequest updateRequest2 = new UpdateDashboardPreviewRequest();
			updateRequest2.Id = 1;
			updateRequest2.Open = false;
			result = (await DashboardController!.UpdateDashbordPreview(updateRequest2)).Value!;
			Assert.AreEqual(false, result.Open);

			results = (await DashboardController!.GetDashbordPreviews(true)).Value!;
			Assert.AreEqual(1, results.Count);
			Assert.AreEqual(2, results[0].Id);
			Assert.AreEqual("Updated test summary", results[0].Summary);

			results = (await DashboardController!.GetDashbordPreviews(false)).Value!;
			Assert.AreEqual(1, results.Count);
			Assert.AreEqual(1, results[0].Id);
		}
	}
}
