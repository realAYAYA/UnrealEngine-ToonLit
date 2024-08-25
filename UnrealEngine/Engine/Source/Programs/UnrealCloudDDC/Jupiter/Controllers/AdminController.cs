// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Threading;
using System.Threading.Tasks;
using Jupiter.Implementation;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Configuration;

namespace Jupiter.Controllers
{
	[ApiController]
	[Route("api/v1/admin")]
	[InternalApiFilter]
	[Authorize]
	public class AdminController : Controller
	{
		private readonly LastAccessServiceReferences _lastAccessService;
		private readonly IRefCleanup _refCleanup;
		private readonly IConfiguration _configuration;
		private readonly IRequestHelper _requestHelper;

		public AdminController(LastAccessServiceReferences lastAccessService, IRefCleanup refCleanup, IConfiguration configuration, IRequestHelper requestHelper)
		{
			_lastAccessService = lastAccessService;
			_refCleanup = refCleanup;
			_configuration = configuration;
			_requestHelper = requestHelper;
		}

		/// <summary>
		/// Report last access times to refs store
		/// </summary>
		/// <remarks>
		/// Manually triggers a aggregation of the last access records and commits these to the refs store. This is done automatically so the only reason to use this endpoint is for debugging purposes.
		/// </remarks>
		/// <returns></returns>
		[HttpPost("startLastAccessRollup")]
		[ProducesResponseType(type: typeof(UpdatedRecordsResponse), 200)]
		public async Task<IActionResult> StartLastAccessRollupAsync()
		{
			ActionResult? result = await _requestHelper.HasAccessForGlobalOperationsAsync(User, new [] { JupiterAclAction.AdminAction });
			if (result != null)
			{
				return result;
			}

			List<(LastAccessRecord, DateTime)>? updatedRecords = await _lastAccessService.ProcessLastAccessRecordsAsync();

			return Ok(new UpdatedRecordsResponse(
				updatedRecords?.Select(tuple => new UpdatedRecordsResponse.UpdatedRecord(tuple.Item1, tuple.Item2))
					.ToList() ??
				new List<UpdatedRecordsResponse.UpdatedRecord>()));
		}

		/// <summary>
		/// Manually run the refs cleanup
		/// </summary>
		/// <remarks>
		/// Manually triggers a cleanup of the refs keys based on last access time. This is done automatically so the only reason to use this endpoint is for debugging purposes.
		/// </remarks>
		/// <returns></returns>
		[HttpPost("refCleanup")]
		public async Task<IActionResult> RefCleanupAsync()
		{
			ActionResult? result = await _requestHelper.HasAccessForGlobalOperationsAsync(User, new [] { JupiterAclAction.AdminAction });
			if (result != null)
			{
				return result;
			}

			int countOfDeletedRecords = await _refCleanup.Cleanup(CancellationToken.None);
			return Ok(new RemovedRefRecordsResponse(countOfDeletedRecords));
		}
		
		/// <summary>
		/// Dumps all settings currently in use
		/// </summary>
		/// <returns></returns>
		[HttpGet("settings")]
		public async Task<IActionResult> SettingsAsync()
		{
			ActionResult? result = await _requestHelper.HasAccessForGlobalOperationsAsync(User, new [] { JupiterAclAction.AdminAction });
			if (result != null)
			{
				return result;
			}

			Dictionary<string, Dictionary<string, object>> settings = new Dictionary<string, Dictionary<string, object>>();

			Dictionary<string, object> ResolveSection(IConfigurationSection section)
			{
				Dictionary<string, object> values = new Dictionary<string, object>();
				foreach (IConfigurationSection childSection in section.GetChildren())
				{
					if (childSection.Value == null)
					{
						values.Add(childSection.Key, ResolveSection(childSection));
					}
					else
					{
						values.Add(childSection.Key, childSection.Value);
					}
				}

				return values;
			}

			foreach (IConfigurationSection section in _configuration.GetChildren())
			{
				Dictionary<string, object> values = ResolveSection(section);
				if (values.Count != 0)
				{
					settings.Add(section.Key, values);
				}
			}

			return new JsonResult(new
			{
				Settings = settings
			}, new System.Text.Json.JsonSerializerOptions
			{
				WriteIndented = true
			})
			{
				StatusCode = (int)HttpStatusCode.OK
			};
		}
	}

	public class RemovedBlobRecords
	{
		public BlobId[] Blobs { get; }

		public RemovedBlobRecords(IEnumerable<BlobId> blobs)
		{
			Blobs = blobs.ToArray();
		}
	}

	public class RemovedRefRecordsResponse
	{
		public RemovedRefRecordsResponse(int countOfRemovedRecords)
		{
			CountOfRemovedRecords = countOfRemovedRecords;
		}

		public int CountOfRemovedRecords { get; }
	}

	public class UpdatedRecordsResponse
	{
		public UpdatedRecordsResponse(List<UpdatedRecord> updatedRecords)
		{
			UpdatedRecords = updatedRecords;
		}

		[System.Diagnostics.CodeAnalysis.SuppressMessage("Design", "CA1034:Nested types should not be visible", Justification = "Only used by serialization")]
		public class UpdatedRecord
		{
			public UpdatedRecord(LastAccessRecord record, DateTime time)
			{
				Record = record;
				Time = time;
			}

			public LastAccessRecord Record { get; }
			public DateTime Time { get; }
		}

		public List<UpdatedRecord> UpdatedRecords { get; }
	}
}
