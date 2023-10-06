// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using Amazon.EC2.Model;

namespace Horde.Server.Dashboard
{
	/// <summary>
	/// Setting information required by dashboard
	/// </summary>
	public class GetDashboardConfigResponse
	{
		/// <summary>
		/// The name of the external issue service
		/// </summary>
		public string? ExternalIssueServiceName { get; set; }

		/// <summary>
		/// The url of the external issue service
		/// </summary>
		public string? ExternalIssueServiceUrl { get; set; }

		/// <summary>
		/// The url of the perforce swarm installation
		/// </summary>
		public string? PerforceSwarmUrl { get; set; }

		/// <summary>
		/// Help email address that users can contact with issues
		/// </summary>
		public string? HelpEmailAddress { get; set; }

		/// <summary>
		/// Help slack channel that users can use for issues
		/// </summary>
		public string? HelpSlackChannel { get; set; }
	}

	/// <summary>
	/// 
	/// </summary>
	public class CreateDashboardPreviewRequest
	{
		/// <summary>
		/// A summary of what the preview item changes
		/// </summary>
		public string Summary { get; set; }

		/// <summary>
		/// The CL the preview was deployed in
		/// </summary>
		public int? DeployedCL { get; set; }

		/// <summary>
		/// An example of the preview site users can view the changes
		/// </summary>
		public string? ExampleLink { get; set; }

		/// <summary>
		/// Optional Link for discussion the preview item
		/// </summary>
		public string? DiscussionLink { get; set; }

		/// <summary>
		/// Optional Link for discussing the preview item
		/// </summary>
		public string? TrackingLink { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		public CreateDashboardPreviewRequest()
		{
			Summary = String.Empty;
		}
	}

	/// <summary>
	/// 
	/// </summary>
	public class UpdateDashboardPreviewRequest
	{
		/// <summary>
		/// The preview item to update
		/// </summary>
		public int Id { get; set; }

		/// <summary>
		/// A summary of what the preview item changes
		/// </summary>
		public string? Summary { get; set; }

		/// <summary>
		/// The CL the preview was deployed in
		/// </summary>
		public int? DeployedCL { get; set; }

		/// <summary>
		/// Whather the preview is under consideration, if false the preview item didn't pass muster
		/// </summary>
		public bool? Open { get; set; }

		/// <summary>
		/// An example of the preview site users can view the changes
		/// </summary>
		public string? ExampleLink { get; set; }

		/// <summary>
		/// Optional Link for discussion the preview item
		/// </summary>
		public string? DiscussionLink { get; set; }

		/// <summary>
		/// Optional Link for discussing the preview item
		/// </summary>
		public string? TrackingLink { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		public UpdateDashboardPreviewRequest()
		{
			Id = 0;
		}
	}


	/// <summary>
	/// 
	/// </summary>
	public class GetDashboardPreviewResponse
	{
		/// <summary>
		/// The unique ID of the preview item
		/// </summary>
		public int Id { get; set; }

		/// <summary>
		/// When the preview item was created
		/// </summary>
		public DateTime CreatedAt { get; set; }

		/// <summary>
		/// A summary of what the preview item changes
		/// </summary>
		public string Summary { get; set; }

		/// <summary>
		/// The CL the preview was deployed in
		/// </summary>
		public int? DeployedCL { get; set; }

		/// <summary>
		/// Whather the preview is under consideration, if false the preview item didn't pass muster
		/// </summary>
		public bool Open { get; set; }

		/// <summary>
		/// An example of the preview site users can view the changes
		/// </summary>
		public string? ExampleLink { get; set; }

		/// <summary>
		/// Optional Link for discussion the preview item
		/// </summary>
		public string? DiscussionLink { get; set; }

		/// <summary>
		/// Optional Link for discussing the preview item
		/// </summary>
		public string? TrackingLink { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="preview"></param>
		public GetDashboardPreviewResponse(IDashboardPreview preview)
		{
			Id = preview.Id;
			CreatedAt = preview.CreatedAt;
			Summary = preview.Summary;	
			DeployedCL= preview.DeployedCL;
			Open = preview.Open;
			ExampleLink = preview.ExampleLink;
			DiscussionLink = preview.DiscussionLink;
			TrackingLink = preview.TrackingLink;
		}
	}
}
