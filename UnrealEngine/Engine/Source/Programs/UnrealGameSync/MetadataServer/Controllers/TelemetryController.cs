// Copyright Epic Games, Inc. All Rights Reserved.

using System.Web.Http;
using MetadataServer.Connectors;
using MetadataServer.Models;

namespace MetadataServer.Controllers
{
    public class TelemetryController : ApiController
    {
		public void Post([FromBody] TelemetryTimingData Data, string Version, string IpAddress)
		{
			SqlConnector.PostTelemetryData(Data, Version, IpAddress);
		}
    }
}
