// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using Microsoft.AspNetCore.Mvc;
using System.ComponentModel.DataAnnotations;
using System.Threading.Tasks;
using Microsoft.AspNetCore.Authorization;
using EpicGames.Horde.Storage;
using Jupiter.Common;
using Microsoft.Extensions.Options;

namespace Jupiter.Controllers
{
	[ApiController]
	[Route("api/v1/auth")]
	[Authorize]
	public class AuthController : ControllerBase
	{
		private readonly IRequestHelper _requestHelper;
		private readonly INamespacePolicyResolver _namespacePolicyResolver;
		private readonly IOptionsMonitor<AuthSettings> _authSettings;

		public AuthController(IRequestHelper requestHelper, INamespacePolicyResolver namespacePolicyResolver, IOptionsMonitor<AuthSettings> authSettings)
		{
			_requestHelper = requestHelper;
			_namespacePolicyResolver = namespacePolicyResolver;
			_authSettings = authSettings;
		}

		[HttpGet("{ns}")]
		public async Task<IActionResult> VerifyAsync(
			[FromRoute][Required] NamespaceId ns
			)
		{
			ActionResult? result = await _requestHelper.HasAccessToNamespaceAsync(User, Request, ns, new[] { JupiterAclAction.ReadObject });
			if (result != null)
			{
				return result;
			}

			return Ok();
		}

		[HttpGet("{ns}/actions")]
		public IActionResult Actions(
			[FromRoute][Required] NamespaceId ns
		)
		{
			NamespacePolicy policy = _namespacePolicyResolver.GetPoliciesForNs(ns);

			List<JupiterAclAction> allowedActions = new List<JupiterAclAction>();
			foreach (AclEntry acl in policy.Acls)
			{
				allowedActions.AddRange(acl.Resolve(User));
			}

			// the root and namespace acls are combined, namespace acls can not override what we define in the root
			foreach (AclEntry acl in _authSettings.CurrentValue.Acls)
			{
				allowedActions.AddRange(acl.Resolve(User));
			}

			return Ok(new JsonResult(new { Actions = allowedActions }));
		}
	}

	public class ActionsResult
	{
		[System.Diagnostics.CodeAnalysis.SuppressMessage("Usage", "CA2227:Collection properties should be read only", Justification = "Used by serialization")]
		public List<JupiterAclAction> Actions { get; set; } = new List<JupiterAclAction>();
	}
}
