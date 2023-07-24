// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using Microsoft.AspNetCore.Mvc;

namespace Horde.Build.Agents.Telemetry
{
	/// <summary>
	/// Controller for the /api/v1/reports endpoint, used for the reports pages
	/// </summary>
	[ApiController]
	[Route("[controller]")]
	public sealed class ReportsController : ControllerBase
	{
		/// <summary>
		/// the Telemetry collection singleton
		/// </summary>
		readonly ITelemetryCollection _telemetryCollection;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="telemetryCollection">The telemetry collection</param>
		public ReportsController(ITelemetryCollection telemetryCollection)
		{
			_telemetryCollection = telemetryCollection;
		}

		/// <summary>
		/// Gets a collection of utilization data including an endpoint and a range
		/// </summary>
		[HttpGet]
		[Route("/api/v1/reports/utilization/{endDate}")]
		public async Task<ActionResult<List<UtilizationTelemetryResponse>>> GetStreamUtilizationData(DateTime endDate, [FromQuery(Name = "Range")] int range, [FromQuery(Name = "TzOffset")] int? tzOffset)
		{
			// Logic here is a bit messy. The client is always passing in a date at midnight
			// If user passes in 12/1/2020 into the date with range of 1, the range should be 12/1/2020:00:00:00 to 12/1/2020:23:59:59
			// Range 2 should be 11/30/2020:00:00:00 to 12/1/2020:23:59:59
			int offset = tzOffset ?? 0;
			DateTimeOffset endDateOffset = new DateTimeOffset(endDate, TimeSpan.FromHours(offset)).Add(new TimeSpan(23, 59, 59));
			DateTimeOffset startDateOffset = endDate.Subtract(new TimeSpan(range - 1, 0, 0, 0));

			List<IUtilizationTelemetry> telemetry = await _telemetryCollection.GetUtilizationTelemetryAsync(startDateOffset.UtcDateTime, endDateOffset.UtcDateTime);

			return telemetry.ConvertAll(telemetry => new UtilizationTelemetryResponse(telemetry));
		}
	}
}
