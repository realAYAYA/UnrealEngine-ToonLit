// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Security.Claims;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;

namespace Jupiter.Controllers
{
	public interface IRequestHelper
	{
		public Task<ActionResult?> HasAccessToNamespaceAsync(ClaimsPrincipal user, HttpRequest request, NamespaceId ns, JupiterAclAction[] aclActions);
		public Task<ActionResult?> HasAccessForGlobalOperationsAsync(ClaimsPrincipal user, JupiterAclAction[] aclActions);
	}

	public class AuthorizationException : Exception
	{
		public ActionResult Result { get; }

		public AuthorizationException(ActionResult result, string errorMessage) : base(errorMessage)
		{
			Result = result;
		}
	}
}
