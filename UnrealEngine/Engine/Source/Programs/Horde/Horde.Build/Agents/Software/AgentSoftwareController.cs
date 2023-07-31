// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.IO;
using System.Threading.Tasks;
using Horde.Build.Acls;
using Horde.Build.Utilities;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Net.Http.Headers;

namespace Horde.Build.Agents.Software
{
	using AgentSoftwareChannelName = StringId<AgentSoftwareChannels>;

	/// <summary>
	/// Controller for the /api/v1/software endpoint
	/// </summary>
	[ApiController]
	[Authorize]
	[Route("[controller]")]
	public class AgentSoftwareController : ControllerBase
	{
		/// <summary>
		/// Singleton instance of the ACL service
		/// </summary>
		private readonly AclService _aclService;

		/// <summary>
		/// Singleton instance of the client service
		/// </summary>
		private readonly AgentSoftwareService _agentSoftwareService;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="aclService">The ACL service</param>
		/// <param name="agentSoftwareService">The client service</param>
		public AgentSoftwareController(AclService aclService, AgentSoftwareService agentSoftwareService)
		{
			_aclService = aclService;
			_agentSoftwareService = agentSoftwareService;
		}

		/// <summary>
		/// Finds all uploaded software matching the given criteria
		/// </summary>
		/// <param name="filter">Filter for the properties to return</param>
		/// <returns>Http response</returns>
		[HttpGet]
		[Route("/api/v1/agentsoftware")]
		[ProducesResponseType(typeof(List<GetAgentSoftwareChannelResponse>), 200)]
		public async Task<ActionResult<List<object>>> FindSoftwareAsync([FromQuery] PropertyFilter? filter = null)
		{
			if (!await _aclService.AuthorizeAsync(AclAction.DownloadSoftware, User))
			{
				return Forbid();
			}

			List<IAgentSoftwareChannel> results = await _agentSoftwareService.FindChannelsAsync();

			List<object> responses = new List<object>();
			foreach (IAgentSoftwareChannel result in results)
			{
				responses.Add(new GetAgentSoftwareChannelResponse(result).ApplyFilter(filter));
			}
			return responses;
		}

		/// <summary>
		/// Finds all uploaded software matching the given criteria
		/// </summary>
		/// <param name="name">Name of the channel to get</param>
		/// <param name="filter">Filter for the properties to return</param>
		/// <returns>Http response</returns>
		[HttpGet]
		[Route("/api/v1/agentsoftware/{name}")]
		[ProducesResponseType(typeof(GetAgentSoftwareChannelResponse), 200)]
		public async Task<ActionResult<object>> FindSoftwareAsync(string name, [FromQuery] PropertyFilter? filter = null)
		{
			if (!await _aclService.AuthorizeAsync(AclAction.DownloadSoftware, User))
			{
				return Forbid();
			}

			IAgentSoftwareChannel? channel = await _agentSoftwareService.GetChannelAsync(new AgentSoftwareChannelName(name));
			if(channel == null)
			{
				return NotFound();
			}

			return new GetAgentSoftwareChannelResponse(channel).ApplyFilter(filter);
		}

		/// <summary>
		/// Uploads a new agent zip file
		/// </summary>
		/// <param name="name">Name of the channel to post to</param>
		/// <param name="file">Zip archive containing the new client software</param>
		/// <returns>Http result code</returns>
		[HttpPost]
		[Route("/api/v1/agentsoftware/{name}/zip")]
		public async Task<ActionResult> SetArchiveAsync(string name, [FromForm] IFormFile file)
		{
			if (!await _aclService.AuthorizeAsync(AclAction.UploadSoftware, User))
			{
				return Forbid();
			}

			byte[] data;
			using (MemoryStream memoryStream = new MemoryStream())
			{
				using (System.IO.Stream stream = file.OpenReadStream())
				{
					await stream.CopyToAsync(memoryStream);
				}
				data = memoryStream.ToArray();
			}

			await _agentSoftwareService.SetArchiveAsync(new AgentSoftwareChannelName(name), User.Identity?.Name, data);
			return Ok();
		}

		/// <summary>
		/// Gets the zip file for a specific channel
		/// </summary>
		/// <param name="name">Name of the channel</param>
		/// <returns>Http response</returns>
		[HttpGet]
		[Route("/api/v1/agentsoftware/{name}/zip")]
		public async Task<ActionResult> GetArchiveAsync(string name)
		{
			if (!await _aclService.AuthorizeAsync(AclAction.DownloadSoftware, User))
			{
				return Forbid();
			}

			byte[]? data = await _agentSoftwareService.GetArchiveAsync(new AgentSoftwareChannelName(name));
			if (data == null)
			{
				return NotFound();
			}

			return new FileStreamResult(new MemoryStream(data), new MediaTypeHeaderValue("application/octet-stream")) { FileDownloadName = $"HordeAgent.zip" };
			
		}
	}
}
