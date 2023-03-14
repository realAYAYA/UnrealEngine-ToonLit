// Copyright Epic Games, Inc. All Rights Reserved.

namespace Horde.Build.Server
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
		/// Response constructor
		/// </summary>
		public GetDashboardConfigResponse()
		{
		}

	}
}

