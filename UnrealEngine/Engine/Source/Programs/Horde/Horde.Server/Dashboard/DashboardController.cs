// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Text;
using System.Threading.Tasks;
using Microsoft.AspNetCore.Authentication;
using Microsoft.AspNetCore.Authentication.Cookies;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Options;
using Horde.Server.Server;
using System.Collections.Generic;
using System.Linq;
using Horde.Server.Agents;
using Horde.Server.Acls;

namespace Horde.Server.Dashboard
{
	/// <summary>	
	/// Dashboard authorization challenge controller	
	/// </summary>	
	[ApiController]
	[Route("[controller]")]
	public class DashboardController : Controller
	{
		/// <summary>
		/// Authentication scheme in use
		/// </summary>
		readonly string _authenticationScheme;

		/// <summary>
		/// Server settings
		/// </summary>
		private readonly ServerSettings _settings;

		private readonly IDashboardPreviewCollection _previewCollection;

		private readonly IOptionsSnapshot<GlobalConfig> _globalConfig;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="previewCollection" />
		/// <param name="serverSettings">Server settings</param>
		/// <param name="globalConfig" />
		public DashboardController(IDashboardPreviewCollection previewCollection, IOptionsMonitor<ServerSettings> serverSettings, IOptionsSnapshot<GlobalConfig> globalConfig)
		{
			_authenticationScheme = AccountController.GetAuthScheme(serverSettings.CurrentValue.AuthMethod);
			_previewCollection = previewCollection;
			_settings = serverSettings.CurrentValue;
			_globalConfig = globalConfig;
		}

		/// <summary>	
		/// Challenge endpoint for the dashboard, using cookie authentication scheme	
		/// </summary>	
		/// <returns>Ok on authorized, otherwise will 401</returns>	
		[HttpGet]
		[Authorize]
		[Route("/api/v1/dashboard/challenge")]
		public StatusCodeResult GetChallenge()
		{
			return Ok();
		}

		/// <summary>
		/// Login to server, redirecting to the specified URL on success
		/// </summary>
		/// <param name="redirect"></param>
		/// <returns></returns>
		[HttpGet]
		[Route("/api/v1/dashboard/login")]
		public IActionResult Login([FromQuery] string? redirect)
		{
			return new ChallengeResult(_authenticationScheme, new AuthenticationProperties { RedirectUri = redirect ?? "/" });
		}

		/// <summary>
		/// Login to server, redirecting to the specified Base64 encoded URL, which fixes some escaping issues on some auth providers, on success
		/// </summary>
		/// <param name="redirect"></param>
		/// <returns></returns>
		[HttpGet]
		[Route("/api/v2/dashboard/login")]
		public IActionResult LoginV2([FromQuery] string? redirect)
		{
			string? redirectUri = null;

			if (redirect != null)
			{
				byte[] data = Convert.FromBase64String(redirect);
				redirectUri = Encoding.UTF8.GetString(data);
			}

			return new ChallengeResult(_authenticationScheme, new AuthenticationProperties { RedirectUri = redirectUri ?? "/index" });
		}

		/// <summary>
		/// Logout of the current account
		/// </summary>
		/// <returns></returns>
		[HttpGet]
		[Route("/api/v1/dashboard/logout")]
		public async Task<StatusCodeResult> Logout()
		{
			await HttpContext.SignOutAsync(CookieAuthenticationDefaults.AuthenticationScheme);
			try
			{
				await HttpContext.SignOutAsync(_authenticationScheme);
			}
			catch
			{
			}

			return Ok();
		}

		/// <summary>
		/// Query all the projects
		/// </summary>
		/// <returns>Config information needed by the dashboard</returns>
		[HttpGet]
		[Authorize]
		[Route("/api/v1/dashboard/config")]
		public ActionResult<GetDashboardConfigResponse> GetDashbordConfig()
		{
			GetDashboardConfigResponse dashboardConfigResponse = new GetDashboardConfigResponse();

			if (_settings.JiraUrl != null)
			{
				dashboardConfigResponse.ExternalIssueServiceName = "Jira";
				dashboardConfigResponse.ExternalIssueServiceUrl = _settings.JiraUrl.ToString().TrimEnd('/');
			}

			if (_settings.P4SwarmUrl != null)
			{
				dashboardConfigResponse.PerforceSwarmUrl = _settings.P4SwarmUrl.ToString().TrimEnd('/');
			}

			dashboardConfigResponse.HelpEmailAddress = _settings.HelpEmailAddress;
			dashboardConfigResponse.HelpSlackChannel = _settings.HelpSlackChannel;

			return dashboardConfigResponse;
		}

		/// <summary>
		/// Create a new dashboard preview item
		/// </summary>
		/// <returns>Config information needed by the dashboard</returns>
		[HttpPost]
		[Authorize]
		[Route("/api/v1/dashboard/preview")]
		public async Task<ActionResult<GetDashboardPreviewResponse>> CreateDashbordPreview([FromBody] CreateDashboardPreviewRequest request)
		{
			if (!_globalConfig.Value.Authorize(AdminAclAction.AdminWrite, User))
			{
				return Forbid();
			}

			IDashboardPreview preview = await _previewCollection.AddPreviewAsync(request.Summary);

			if (!String.IsNullOrEmpty(request.ExampleLink) || !String.IsNullOrEmpty(request.DiscussionLink) || !String.IsNullOrEmpty(request.TrackingLink))
			{
				IDashboardPreview? updated = await _previewCollection.UpdatePreviewAsync(preview.Id, null, null, null, request.ExampleLink, request.DiscussionLink, request.TrackingLink);
				if (updated == null) 
				{
					return NotFound(preview.Id);
				}

				return new GetDashboardPreviewResponse(updated);
			}

			return new GetDashboardPreviewResponse(preview);
		}

		/// <summary>
		/// Update a dashboard preview item
		/// </summary>
		/// <returns>Config information needed by the dashboard</returns>
		[HttpPut]
		[Authorize]
		[Route("/api/v1/dashboard/preview")]
		public async Task<ActionResult<GetDashboardPreviewResponse>> UpdateDashbordPreview([FromBody] UpdateDashboardPreviewRequest request)
		{
			if (!_globalConfig.Value.Authorize(AdminAclAction.AdminWrite, User))
			{
				return Forbid();
			}

			IDashboardPreview? preview = await _previewCollection.UpdatePreviewAsync(request.Id, request.Summary, request.DeployedCL, request.Open, request.ExampleLink, request.DiscussionLink, request.TrackingLink);
			
			if (preview == null)
			{
				return NotFound(request.Id);
			}			

			return new GetDashboardPreviewResponse(preview);
		}

		/// <summary>
		/// Query dashboard preview items
		/// </summary>
		/// <returns>Config information needed by the dashboard</returns>
		[HttpGet]
		[Authorize]
		[Route("/api/v1/dashboard/previews")]
		public async Task<ActionResult<List<GetDashboardPreviewResponse>>> GetDashbordPreviews([FromQuery] bool open = true)
		{			
			List <IDashboardPreview> previews = await _previewCollection.FindPreviewsAsync(open);			
			return previews.Select(p => new GetDashboardPreviewResponse(p)).ToList();
		}
	}
}
