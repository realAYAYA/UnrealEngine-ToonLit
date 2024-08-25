// Copyright Epic Games, Inc. All Rights Reserved.

using System.Net;
using EpicGames.Core;
using Horde.Server.Utilities;
using Microsoft.AspNetCore.Diagnostics;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Logging;

namespace Horde.Server.Server
{
	/// <summary>
	/// Controller managing account status
	/// </summary>
	[ApiController]
	[Route("[controller]")]
	public class ExceptionController : HordeControllerBase
	{
		/// <summary>
		/// Outputs a diagnostic error response for an exception
		/// </summary>
		[Route("/api/v1/exception")]
		[ApiExplorerSettings(IgnoreApi = true)]
		public ActionResult Exception()
		{
			IExceptionHandlerPathFeature? feature = HttpContext.Features.Get<IExceptionHandlerPathFeature>();

			int statusCode = (int)HttpStatusCode.InternalServerError;

			LogEvent logEvent;
			if (feature == null)
			{
				logEvent = LogEvent.Create(LogLevel.Error, "Exception handler path feature is missing.");
			}
			else if (feature.Error == null)
			{
				logEvent = LogEvent.Create(LogLevel.Information, "No error code.");
			}
			else if (feature.Error is StructuredHttpException structuredHttpEx)
			{
				(logEvent, statusCode) = (structuredHttpEx.ToLogEvent(), structuredHttpEx.StatusCode);
			}
			else if (feature.Error is StructuredException structuredEx)
			{
				logEvent = structuredEx.ToLogEvent();
			}
			else
			{
				logEvent = LogEvent.Create(LogLevel.Error, default, feature.Error, "Unhandled exception: {Message}", feature.Error.Message);
			}

			return new ObjectResult(logEvent) { StatusCode = statusCode };
		}
	}
}
