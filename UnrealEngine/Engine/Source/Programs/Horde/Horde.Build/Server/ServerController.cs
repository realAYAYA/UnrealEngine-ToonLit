// Copyright Epic Games, Inc. All Rights Reserved.

using System.Diagnostics;
using System.Reflection;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Options;

namespace Horde.Build.Server
{
	/// <summary>
	/// Controller managing account status
	/// </summary>
	[ApiController]
	[Authorize]
	[Route("[controller]")]
	public class ServerController : ControllerBase
	{

		/// <summary>
		/// Settings for the server
		/// </summary>
		readonly IOptionsMonitor<ServerSettings> _settings;

		/// <summary>
		/// Constructor
		/// </summary>
		public ServerController(IOptionsMonitor<ServerSettings> settings)
		{
			_settings = settings;
		}
		
		/// <summary>
		/// Get server version
		/// </summary>
		[HttpGet]
		[Route("/api/v1/server/version")]
		public ActionResult GetVersionAsync()
		{
			FileVersionInfo fileVersionInfo = FileVersionInfo.GetVersionInfo(Assembly.GetExecutingAssembly().Location);
			return Ok(fileVersionInfo.ProductVersion);
		}		

		/// <summary>
		/// Get server information
		/// </summary>
		[HttpGet]
		[Route("/api/v1/server/info")]
		[ProducesResponseType(typeof(GetServerInfoResponse), 200)]
		public ActionResult<GetServerInfoResponse> GetServerInfo()
		{
			return new GetServerInfoResponse(_settings.CurrentValue.SingleInstance);
		}
	}
}
