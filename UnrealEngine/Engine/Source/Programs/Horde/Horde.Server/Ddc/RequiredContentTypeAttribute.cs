// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Linq;
using Microsoft.AspNetCore.Mvc.ActionConstraints;
using Microsoft.Extensions.Primitives;

#pragma warning disable CS1591

namespace Horde.Server.Ddc
{
	[AttributeUsage(AttributeTargets.Method)]
	public sealed class RequiredContentTypeAttribute : Attribute, IActionConstraint
	{
		public string[] MediaTypeNames { get; }

		public RequiredContentTypeAttribute(params string[] mediaTypeNames)
		{
			MediaTypeNames = mediaTypeNames;
		}

		public int Order => 0;

		public bool Accept(ActionConstraintContext context)
		{
			StringValues contentTypeHeader = context.RouteContext.HttpContext.Request.Headers["Content-Type"];

			bool valid = contentTypeHeader.Any(s => MediaTypeNames.Contains(s, StringComparer.InvariantCultureIgnoreCase));
			return valid;
		}
	}
}
