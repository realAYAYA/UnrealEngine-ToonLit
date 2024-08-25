// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;
using System.Linq;
using System.Text.Json.Serialization;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using EpicGames.Serialization;
using Jupiter.Common;
using Jupiter.Implementation;
using Jupiter.Implementation.TransactionLog;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Options;

namespace Jupiter.Controllers
{
	[ApiController]
	[Route("api/v1/replication-log")]
	[InternalApiFilter]
	[Authorize]
	public class ReplicationLogController : ControllerBase
	{
		private readonly IServiceProvider _provider;
		private readonly IRequestHelper _requestHelper;
		private readonly IReplicationLog _replicationLog;
		private readonly IOptionsMonitor<SnapshotSettings> _snapshotSettings;

		public ReplicationLogController(IServiceProvider provider, IRequestHelper requestHelper, IReplicationLog replicationLog, IOptionsMonitor<SnapshotSettings> snapshotSettings)
		{
			_provider = provider;
			_requestHelper = requestHelper;
			_replicationLog = replicationLog;
			_snapshotSettings = snapshotSettings;
		}

		[HttpGet("snapshots/{ns}")]
		[ProducesDefaultResponseType]
		[ProducesResponseType(type: typeof(ProblemDetails), 400)]
		public async Task<IActionResult> GetSnapshotsAsync(
			[Required] NamespaceId ns
		)
		{
			ActionResult? result = await _requestHelper.HasAccessToNamespaceAsync(User, Request, ns, new [] { JupiterAclAction.ReadTransactionLog });
			if (result != null)
			{
				return result;
			}

			return Ok(new ReplicationLogSnapshots(await _replicationLog.GetSnapshotsAsync(ns).ToListAsync()));
		}

		
		[HttpPost("snapshots/{ns}/create")]
		[ProducesDefaultResponseType]
		[ProducesResponseType(type: typeof(ProblemDetails), 400)]
		public async Task<IActionResult> CreateSnapshotAsync(
			[Required] NamespaceId ns
		)
		{
			ActionResult? result = await _requestHelper.HasAccessToNamespaceAsync(User, Request, ns, new [] { JupiterAclAction.WriteTransactionLog });
			if (result != null)
			{
				return result;
			}

			ReplicationLogSnapshotBuilder builder = ActivatorUtilities.CreateInstance<ReplicationLogSnapshotBuilder>(_provider);
			BlobId snapshotBlob = await builder.BuildSnapshotAsync(ns, _snapshotSettings.CurrentValue.SnapshotStorageNamespace, CancellationToken.None);
			return Ok(new SnapshotCreatedResponse(snapshotBlob));
		}

		[HttpGet("incremental/{ns}")]
		[ProducesDefaultResponseType]
		[ProducesResponseType(type: typeof(ProblemDetails), 400)]
		public async Task<IActionResult> GetIncrementalEventsAsync(
			[Required] NamespaceId ns,
			[FromQuery] string? lastBucket,
			[FromQuery] Guid? lastEvent,
			[FromQuery] int count = 1000
		)
		{
			ActionResult? result = await _requestHelper.HasAccessToNamespaceAsync(User, Request, ns, new [] { JupiterAclAction.ReadTransactionLog });
			if (result != null)
			{
				return result;
			}

			if (((lastBucket == null && lastEvent.HasValue) || (lastBucket != null && !lastEvent.HasValue)) && lastBucket != "now")
			{
				return BadRequest(new ProblemDetails
				{
					Title = $"Both bucket and event has to be specified, or omit both.",
				});
			}

			try
			{
				IAsyncEnumerable<ReplicationLogEvent> events = _replicationLog.GetAsync(ns, lastBucket, lastEvent);

				List<ReplicationLogEvent> l = await events.Take(count).ToListAsync();
				return Ok(new ReplicationLogEvents(l));
			}
			catch (IncrementalLogNotAvailableException)
			{
				// failed to resume from the incremental log, check for a snapshot instead
				SnapshotInfo? snapshot = await _replicationLog.GetLatestSnapshotAsync(ns);
				if (snapshot != null)
				{
					// no log file is available
					return BadRequest(new ProblemDetails
					{
						Title = $"Log file is not available, use snapshot {snapshot.SnapshotBlob} instead",
						Type = ProblemTypes.UseSnapshot,
						Extensions = { { "SnapshotId", snapshot.SnapshotBlob }, { "BlobNamespace", snapshot.BlobNamespace },  }
					});
				}

				// if no snapshot is available we just give up, they can always reset the replication to the default behavior by not sending in lastBucket and lastEvent

				return BadRequest(new ProblemDetails
				{
					Title = $"No snapshot or bucket found for namespace \"{ns}\"",
				});
			}
			catch (NamespaceNotFoundException)
			{
				return NotFound(new ProblemDetails
				{
					Title = $"Namespace {ns} was not found",
				});
			}
		}
	}

	public class SnapshotCreatedResponse
	{
		public SnapshotCreatedResponse()
		{
			SnapshotBlobId = null!;
		}

		public SnapshotCreatedResponse(BlobId snapshotBlob)
		{
			SnapshotBlobId = snapshotBlob;
		}

		[CbField("snapshotBlobId")]
		public BlobId SnapshotBlobId { get; set; }
	}

	public class ReplicationLogSnapshots
	{
		public ReplicationLogSnapshots()
		{
			Snapshots = new List<SnapshotInfo>();
		}

		[JsonConstructor]
		public ReplicationLogSnapshots(List<SnapshotInfo> snapshots)
		{
			Snapshots = snapshots;
		}

		[System.Diagnostics.CodeAnalysis.SuppressMessage("Usage", "CA2227:Collection properties should be read only", Justification = "Used by serialization")]
		public List<SnapshotInfo> Snapshots { get; set; }
	}

	public class ReplicationLogEvents
	{
		public ReplicationLogEvents()
		{
			Events = new List<ReplicationLogEvent>();
		}

		[JsonConstructor]
		public ReplicationLogEvents(List<ReplicationLogEvent> events)
		{
			Events = events;
		}

		[System.Diagnostics.CodeAnalysis.SuppressMessage("Usage", "CA2227:Collection properties should be read only", Justification = "Used by serialization")]
		public List<ReplicationLogEvent> Events { get; set; }
	}
}
