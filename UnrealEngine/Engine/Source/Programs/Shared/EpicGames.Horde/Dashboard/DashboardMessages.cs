// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using EpicGames.Horde.Common;
using EpicGames.Horde.Server;

#pragma warning disable CA2227 // Collection properties should be read only

namespace EpicGames.Horde.Dashboard
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

		/// <summary>
		/// The auth method in use
		/// </summary>
		public AuthMethod AuthMethod { get; set; }

		/// <summary>
		/// Device problem cooldown in minutes
		/// </summary>
		public int DeviceProblemCooldownMinutes { get; set; }

		/// <summary>
		/// Categories to display on the agents page
		/// </summary>
		public List<GetDashboardAgentCategoryResponse> AgentCategories { get; set; } = new List<GetDashboardAgentCategoryResponse>();

		/// <summary>
		/// Categories to display on the pools page
		/// </summary>
		public List<GetDashboardPoolCategoryResponse> PoolCategories { get; set; } = new List<GetDashboardPoolCategoryResponse>();

		/// <summary>
		/// Telemetry to display on the telemetry page
		/// </summary>
		public List<GetTelemetryViewResponse> TelemetryViews { get; set; } = new List<GetTelemetryViewResponse>();

	}

	/// <summary>
	/// Describes a category for the pools page
	/// </summary>
	public class GetDashboardPoolCategoryResponse
	{
		/// <summary>
		/// Title for the tab
		/// </summary>
		public string Name { get; set; } = "Unnamed";

		/// <summary>
		/// Condition for pools to be included in this category
		/// </summary>
		public Condition? Condition { get; set; }
	}

	/// <summary>
	/// Describes a category for the agents page
	/// </summary>
	public class GetDashboardAgentCategoryResponse
	{
		/// <summary>
		/// Title for the tab
		/// </summary>
		public string Name { get; set; } = "Unnamed";

		/// <summary>
		/// Condition for agents to be included in this category
		/// </summary>
		public Condition? Condition { get; set; }
	}

	/// <summary>
	/// 
	/// </summary>
	public class CreateDashboardPreviewRequest
	{
		/// <summary>
		/// A summary of what the preview item changes
		/// </summary>
		public string Summary { get; set; } = String.Empty;

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
	}

	/// <summary>
	/// Dashboard preview item response
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
		public string Summary { get; set; } = String.Empty;

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
	}

	/// <summary>
	/// Dashboard challenge response
	/// </summary>
	public class GetDashboardChallengeResponse
	{
		/// <summary>
		/// Whether first time setup needs to run
		/// </summary>
		public bool NeedsFirstTimeSetup { get; set; } = false;

		/// <summary>
		/// Whether the user needs to authorize
		/// </summary>
		public bool NeedsAuthorization { get; set; } = true;

	}

	#region Telemetry

	/// <summary>
	/// Metric attached to a telemetry chart
	/// </summary>
	public class GetTelemetryChartMetricResponse
	{
		/// <summary>
		/// Associated metric id
		/// </summary>		
		public string MetricId { get; set; } = null!;

		/// <summary>
		/// The threshold for KPI values
		/// </summary>
		public int? Threshold { get; set; }

		/// <summary>
		/// The metric alias for display purposes
		/// </summary>
		public string? Alias { get; set; }
	}

	/// <summary>
	/// Telemetry chart configuraton
	/// </summary>
	public class GetTelemetryChartResponse
	{
		/// <summary>
		/// The name of the chart, will be displayed on the dashboard
		/// </summary>		
		public string Name { get; set; } = null!;

		/// <summary>
		/// The unit to display
		/// </summary>
		public string Display { get; set; } = null!;

		/// <summary>
		/// The graph type 
		/// </summary>
		public string Graph { get; set; } = null!;

		/// <summary>
		/// List of configured metrics
		/// </summary>
		public List<GetTelemetryChartMetricResponse> Metrics { get; set; } = new List<GetTelemetryChartMetricResponse>();

		/// <summary>
		/// The min unit value for clamping chart
		/// </summary>
		public int? Min { get; set; }

		/// <summary>
		/// The max unit value for clamping chart
		/// </summary>
		public int? Max { get; set; }
	}

	/// <summary>
	/// A chart categody, will be displayed on the dashbord under an associated pivot
	/// </summary>
	public class GetTelemetryCategoryResponse
	{
		/// <summary>
		/// The name of the category
		/// </summary>		
		public string Name { get; set; } = null!;

		/// <summary>
		/// The charts contained within the category
		/// </summary>
		public List<GetTelemetryChartResponse> Charts { get; set; } = new List<GetTelemetryChartResponse> { };
	}

	/// <summary>
	/// A telemetry view variable used for filtering the charting data
	/// </summary>
	public class GetTelemetryVariableResponse
	{
		/// <summary>
		/// The name of the variable for display purposes
		/// </summary>
		public string Name { get; set; } = null!;

		/// <summary>
		/// The associated data group attached to the variable 
		/// </summary>
		public string Group { get; set; } = null!;

		/// <summary>
		/// The default values to select
		/// </summary>
		public List<string> Defaults { get; set; } = new List<string> { };
	}

	/// <summary>
	/// A telemetry view of related metrics, divided into categofies
	/// </summary>
	public class GetTelemetryViewResponse
	{
		/// <summary>
		/// Identifier for the view
		/// </summary>
		public string Id { get; set; } = null!;

		/// <summary>
		/// The name of the view
		/// </summary>
		public string Name { get; set; } = null!;

		/// <summary>
		/// The telemetry store the view uses
		/// </summary>
		public string TelemetryStoreId { get; set; } = null!;

		/// <summary>
		///  The variables used to filter the view data
		/// </summary>
		public List<GetTelemetryVariableResponse> Variables { get; set; } = new List<GetTelemetryVariableResponse> { };

		/// <summary>
		/// The categories contained within the view
		/// </summary>
		public List<GetTelemetryCategoryResponse> Categories { get; set; } = new List<GetTelemetryCategoryResponse> { };
	}
	#endregion
}
