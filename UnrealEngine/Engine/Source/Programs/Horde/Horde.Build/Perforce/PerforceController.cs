// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Net;
using System.Threading.Tasks;
using Horde.Build.Acls;
using Horde.Build.Server;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Logging;

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
		/// <summary>
		/// The database service instance
		/// </summary>
		private readonly MongoService _mongoService;

		/// <summary>
		/// The ACL service instance
		/// </summary>
		private readonly AclService _aclService;

		/// <summary>
		/// Load balancer instance
		/// </summary>
		private readonly PerforceLoadBalancer _perforceLoadBalancer;

		/// <summary>
		/// Constructor
		/// </summary>
		public PerforceController(MongoService mongoService, AclService aclService, PerforceLoadBalancer perforceLoadBalancer)
		{
			_mongoService = mongoService;
			_aclService = aclService;
			_perforceLoadBalancer = perforceLoadBalancer;
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
		public async Task<ActionResult<List<PerforceCluster>>> GetPerforceSettingsAsync()
		{
			if (!await _aclService.AuthorizeAsync(AclAction.AdminRead, User))
			{
				return Forbid();
			}

			Globals globals = await _mongoService.GetGlobalsAsync();
			return globals.PerforceClusters;
		}
	}

	/// <summary>
	/// Controller for Perforce triggers and callbacks
	/// </summary>
	[ApiController]
	[Route("[controller]")]
	public class PublicPerforceController : ControllerBase
	{
		/// <summary>
		/// Logger instance
		/// </summary>
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
