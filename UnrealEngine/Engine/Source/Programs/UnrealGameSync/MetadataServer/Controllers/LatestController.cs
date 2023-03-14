// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Web.Http;
using MetadataServer.Connectors;
using MetadataServer.Models;

namespace MetadataServer.Controllers
{
    public class LatestController : ApiController
    {
		public LatestData Get(string Project = null)
		{
			return SqlConnector.GetLastIds(Project);
		}
	}
}
