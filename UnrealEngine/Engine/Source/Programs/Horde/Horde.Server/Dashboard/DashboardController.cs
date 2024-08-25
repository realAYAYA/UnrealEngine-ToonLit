// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Accounts;
using EpicGames.Horde.Dashboard;
using EpicGames.Horde.Server;
using Horde.Server.Accounts;
using Horde.Server.Server;
using Horde.Server.Telemetry;
using Horde.Server.Utilities;
using Microsoft.AspNetCore.Authentication;
using Microsoft.AspNetCore.Authentication.Cookies;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Options;

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
		readonly ServerSettings _settings;

		readonly IDashboardPreviewCollection _previewCollection;

		readonly IAccountCollection _hordeAccounts;

		readonly IOptionsSnapshot<GlobalConfig> _globalConfig;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="previewCollection" />
		/// <param name="hordeAccounts" />
		/// <param name="serverSettings">Server settings</param>
		/// <param name="globalConfig" />
		public DashboardController(IDashboardPreviewCollection previewCollection, IAccountCollection hordeAccounts, IOptionsMonitor<ServerSettings> serverSettings, IOptionsSnapshot<GlobalConfig> globalConfig)
		{
			_authenticationScheme = AccountController.GetAuthScheme(serverSettings.CurrentValue.AuthMethod);
			_previewCollection = previewCollection;
			_hordeAccounts = hordeAccounts;
			_settings = serverSettings.CurrentValue;
			_globalConfig = globalConfig;
		}

		/// <summary>	
		/// Challenge endpoint for the dashboard, using cookie authentication scheme	
		/// </summary>	
		/// <returns>Ok on authorized, otherwise will 401</returns>	
		[HttpGet]
		[Route("/api/v1/dashboard/challenge")]
		public async Task<IActionResult> GetChallengeAsync()
		{
			bool needsFirstTimeSetup = false;
			if (_settings.AuthMethod == AuthMethod.Horde)
			{
				IAccount? account = await _hordeAccounts.FindByLoginAsync("Admin");
				if (account == null)
				{
					needsFirstTimeSetup = true;
				}
			}

			return Ok(new GetDashboardChallengeResponse { NeedsFirstTimeSetup = needsFirstTimeSetup, NeedsAuthorization = User.Identity == null || !User.Identity.IsAuthenticated });
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
			if (_settings.AuthMethod == AuthMethod.Horde)
			{
				return Redirect("/login" + (redirect == null ? String.Empty : $"?redirect={redirect}"));
			}

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
		/// /// <param name="dashboard"></param>
		/// <returns></returns>
		[HttpGet]
		[Route("/api/v1/dashboard/logout")]
		public async Task<ActionResult> LogoutAsync([FromQuery] bool? dashboard)
		{
			await HttpContext.SignOutAsync(CookieAuthenticationDefaults.AuthenticationScheme);
			try
			{
				await HttpContext.SignOutAsync(_authenticationScheme);
			}
			catch
			{
			}

			if (dashboard == false && _settings.AuthMethod == AuthMethod.Horde)
			{
				return Redirect("/");
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

			dashboardConfigResponse.AuthMethod = _settings.AuthMethod;

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

			dashboardConfigResponse.DeviceProblemCooldownMinutes = _settings.DeviceProblemCooldownMinutes;

			foreach (DashboardAgentCategoryConfig category in _globalConfig.Value.Dashboard.AgentCategories)
			{
				dashboardConfigResponse.AgentCategories.Add(new GetDashboardAgentCategoryResponse { Name = category.Name, Condition = category.Condition });
			}

			foreach (DashboardPoolCategoryConfig category in _globalConfig.Value.Dashboard.PoolCategories)
			{
				dashboardConfigResponse.PoolCategories.Add(new GetDashboardPoolCategoryResponse { Name = category.Name, Condition = category.Condition });
			}

			if (_globalConfig.Value.Authorize(TelemetryAclAction.QueryMetrics, User))
			{
				foreach (TelemetryViewConfig telemetry in _globalConfig.Value.Dashboard.Analytics)
				{
					GetTelemetryViewResponse rview = new GetTelemetryViewResponse();
					rview.Id = telemetry.Id.ToString();
					rview.Name = telemetry.Name;
					rview.TelemetryStoreId = telemetry.TelemetryStoreId.ToString();

					foreach (TelemetryVariableConfig variable in telemetry.Variables)
					{
						rview.Variables.Add(new GetTelemetryVariableResponse { Name = variable.Name, Group = variable.Group, Defaults = variable.Defaults });
					}

					foreach (TelemetryCategoryConfig category in telemetry.Categories)
					{
						GetTelemetryCategoryResponse rcategory = new GetTelemetryCategoryResponse { Name = category.Name };

						foreach (TelemetryChartConfig chart in category.Charts)
						{
							GetTelemetryChartResponse rchart = new GetTelemetryChartResponse { Name = chart.Name, Display = chart.Display.ToString(), Graph = chart.Graph.ToString(), Min = chart.Min, Max = chart.Max, Metrics = new List<GetTelemetryChartMetricResponse>() };

							foreach (TelemetryChartMetricConfig metric in chart.Metrics)
							{
								rchart.Metrics.Add(new GetTelemetryChartMetricResponse { MetricId = metric.Id.ToString(), Threshold = metric.Threshold, Alias = metric.Alias });
							}

							rcategory.Charts.Add(rchart);
						}

						rview.Categories.Add(rcategory);
					}

					dashboardConfigResponse.TelemetryViews.Add(rview);
				}
			}

			return dashboardConfigResponse;
		}

		/// <summary>
		/// Create a new dashboard preview item
		/// </summary>
		/// <returns>Config information needed by the dashboard</returns>
		[HttpPost]
		[Authorize]
		[Route("/api/v1/dashboard/preview")]
		public async Task<ActionResult<GetDashboardPreviewResponse>> CreateDashbordPreviewAsync([FromBody] CreateDashboardPreviewRequest request, CancellationToken cancellationToken = default)
		{
			if (!_globalConfig.Value.Authorize(AdminAclAction.AdminWrite, User))
			{
				return Forbid();
			}

			IDashboardPreview preview = await _previewCollection.AddPreviewAsync(request.Summary, cancellationToken);

			if (!String.IsNullOrEmpty(request.ExampleLink) || !String.IsNullOrEmpty(request.DiscussionLink) || !String.IsNullOrEmpty(request.TrackingLink))
			{
				IDashboardPreview? updated = await _previewCollection.UpdatePreviewAsync(preview.Id, null, null, null, request.ExampleLink, request.DiscussionLink, request.TrackingLink, cancellationToken);
				if (updated == null)
				{
					return NotFound(preview.Id);
				}

				return CreatePreviewResponse(updated);
			}

			return CreatePreviewResponse(preview);
		}

		/// <summary>
		/// Update a dashboard preview item
		/// </summary>
		/// <returns>Config information needed by the dashboard</returns>
		[HttpPut]
		[Authorize]
		[Route("/api/v1/dashboard/preview")]
		public async Task<ActionResult<GetDashboardPreviewResponse>> UpdateDashbordPreviewAsync([FromBody] UpdateDashboardPreviewRequest request, CancellationToken cancellationToken = default)
		{
			if (!_globalConfig.Value.Authorize(AdminAclAction.AdminWrite, User))
			{
				return Forbid();
			}

			IDashboardPreview? preview = await _previewCollection.UpdatePreviewAsync(request.Id, request.Summary, request.DeployedCL, request.Open, request.ExampleLink, request.DiscussionLink, request.TrackingLink, cancellationToken);

			if (preview == null)
			{
				return NotFound(request.Id);
			}

			return CreatePreviewResponse(preview);
		}

		/// <summary>
		/// Query dashboard preview items
		/// </summary>
		/// <returns>Config information needed by the dashboard</returns>
		[HttpGet]
		[Authorize]
		[Route("/api/v1/dashboard/previews")]
		public async Task<ActionResult<List<GetDashboardPreviewResponse>>> GetDashbordPreviewsAsync([FromQuery] bool open = true, CancellationToken cancellationToken = default)
		{
			List<IDashboardPreview> previews = await _previewCollection.FindPreviewsAsync(open, cancellationToken);
			return previews.Select(CreatePreviewResponse).ToList();
		}

		static GetDashboardPreviewResponse CreatePreviewResponse(IDashboardPreview preview)
		{
			GetDashboardPreviewResponse response = new GetDashboardPreviewResponse();
			response.Id = preview.Id;
			response.CreatedAt = preview.CreatedAt;
			response.Summary = preview.Summary;
			response.DeployedCL = preview.DeployedCL;
			response.Open = preview.Open;
			response.ExampleLink = preview.ExampleLink;
			response.DiscussionLink = preview.DiscussionLink;
			response.TrackingLink = preview.TrackingLink;
			return response;
		}

		/// <summary>
		/// Returns a list of valid user-defined groups from the current server config. These are any claims with
		/// the <see cref="HordeClaimTypes.Group"/> type.
		/// </summary>
		[HttpGet]
		[Authorize]
		[Route("/api/v1/dashboard/accountgroups")]
		public ActionResult<List<AccountClaimMessage>> GetAccountGroupClaims()
		{
			if (!_globalConfig.Value.Authorize(AccountAclAction.CreateAccount, User) && !_globalConfig.Value.Authorize(AccountAclAction.UpdateAccount, User))
			{
				return Forbid();
			}

			return Ok(_globalConfig.Value.GetValidAccountGroupClaims().Select(x => new AccountClaimMessage(HordeClaimTypes.Group, x)).ToList());
		}
	}
}
