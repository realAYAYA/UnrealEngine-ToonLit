// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Text;
using System.Threading.Tasks;
using Microsoft.AspNetCore.Authentication;
using Microsoft.AspNetCore.Authentication.Cookies;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Options;

namespace Horde.Build.Server
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


		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="serverSettings">Server settings</param>
		public DashboardController(IOptionsMonitor<ServerSettings> serverSettings)
		{
			_authenticationScheme = AccountController.GetAuthScheme(serverSettings.CurrentValue.AuthMethod);
			_settings = serverSettings.CurrentValue;
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

	}
}
