// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Web.Http;
using MetadataServer.Connectors;
using MetadataServer.Models;

namespace MetadataServer.Controllers
{
    public class ErrorController : ApiController
    {
		public List<TelemetryErrorData> Get(int Records = 10)
		{
			return SqlConnector.GetErrorData(Records);
		}
		public void Post([FromBody] TelemetryErrorData Data, string Version, string IpAddress)
		{
			SqlConnector.PostErrorData(Data, Version, IpAddress);
		}
    }
}
