// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Net;
using System.Threading.Tasks;
using Horde.Build.Acls;
using Horde.Build.Server;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Horde.Build.Perforce
{
	/// <summary>
	/// Controller for Perforce triggers and callbacks
	/// </summary>
	[Authorize]
	[ApiController]
	[Route("[controller]")]
	public class PerforceController : ControllerBase
	{
		private readonly PerforceLoadBalancer _perforceLoadBalancer;
		private readonly IOptionsSnapshot<GlobalConfig> _globalConfig;

		/// <summary>
		/// Constructor
		/// </summary>
		public PerforceController(PerforceLoadBalancer perforceLoadBalancer, IOptionsSnapshot<GlobalConfig> globalConfig)
		{
			_perforceLoadBalancer = perforceLoadBalancer;
			_globalConfig = globalConfig;
		}

		/// <summary>
		/// Get the current server status
		/// </summary>
		/// <returns></returns>
		[HttpGet]
		[Route("/api/v1/perforce/status")]
		public async Task<ActionResult<List<object>>> GetStatusAsync()
		{
			List<IPerforceServer> servers = await _perforceLoadBalancer.GetServersAsync();

			List<object> responses = new List<object>();
			foreach (IPerforceServer server in servers)
			{
				responses.Add(new { server.ServerAndPort, server.BaseServerAndPort, server.Cluster, server.NumLeases, server.Status, server.Detail, server.LastUpdateTime });
			}
			return responses;
		}

		/// <summary>
		/// Gets the current perforce settinsg
		/// </summary>
		/// <returns>List of Perforce clusters</returns>
		[HttpGet]
		[Route("/api/v1/perforce/settings")]
		public ActionResult<List<PerforceCluster>> GetPerforceSettingsAsync()
		{
			GlobalConfig globalConfig = _globalConfig.Value;

			if (!globalConfig.Authorize(AclAction.AdminRead, User))
			{
				return Forbid();
			}

			return globalConfig.PerforceClusters;
		}
	}

	/// <summary>
	/// Controller for Perforce triggers and callbacks
	/// </summary>
	[ApiController]
	[Route("[controller]")]
	public class PublicPerforceController : ControllerBase
	{
		private readonly ILogger<PerforceController> _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public PublicPerforceController(ILogger<PerforceController> logger)
		{
			_logger = logger;
		}

		/// <summary>
		/// Endpoint which trigger scripts in Perforce will call
		/// </summary>
		/// <returns>200 OK on success</returns>
		[HttpPost]
		[Route("/api/v1/perforce/trigger/{type}")]
		public ActionResult TriggerCallback(string type, [FromQuery] long? changelist = null, [FromQuery(Name = "user")] string? perforceUser = null)
		{
			// Currently just a placeholder until correct triggers are in place.
			_logger.LogDebug("Received Perforce trigger callback. Type={Type} Changelist={Changelist} User={User}", type, changelist, perforceUser);
			string content = "{\"message\": \"Trigger received\"}";
			return new ContentResult { ContentType = "text/plain", StatusCode = (int)HttpStatusCode.OK, Content = content };
		}
	}
}
