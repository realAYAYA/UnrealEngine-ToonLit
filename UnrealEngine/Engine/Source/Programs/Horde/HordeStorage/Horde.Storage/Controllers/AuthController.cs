// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using Microsoft.AspNetCore.Mvc;
using System.ComponentModel.DataAnnotations;
using System.Threading.Tasks;
using Microsoft.AspNetCore.Authorization;
using EpicGames.Horde.Storage;
using Jupiter;
using Jupiter.Common;
using Microsoft.Extensions.Options;
using Newtonsoft.Json;
using Newtonsoft.Json.Converters;

namespace Horde.Storage.Controllers
{
    [ApiController]
    [Route("api/v1/auth")]
    [Authorize]
    public class AuthController : ControllerBase
    {
        private readonly RequestHelper _requestHelper;
        private readonly INamespacePolicyResolver _namespacePolicyResolver;
        private readonly IOptionsMonitor<AuthSettings> _authSettings;

        public AuthController(RequestHelper requestHelper, INamespacePolicyResolver namespacePolicyResolver, IOptionsMonitor<AuthSettings> authSettings)
        {
            _requestHelper = requestHelper;
            _namespacePolicyResolver = namespacePolicyResolver;
            _authSettings = authSettings;
        }

        [HttpGet("{ns}")]
        public async Task<IActionResult> Verify(
            [FromRoute] [Required] NamespaceId ns
            )
        {
            ActionResult? result = await _requestHelper.HasAccessToNamespace(User, Request, ns, new [] { AclAction.ReadObject });
            if (result != null)
            {
                return result;
            }

            return Ok();
        }

        [HttpGet("{ns}/actions")]
        public IActionResult Actions(
            [FromRoute] [Required] NamespaceId ns
        )
        {
            NamespacePolicy policy = _namespacePolicyResolver.GetPoliciesForNs(ns);

            List<AclAction> allowedActions = new List<AclAction>();
            foreach (AclEntry acl in policy.Acls)
            {
                allowedActions.AddRange(acl.Resolve(User));
            }

            // the root and namespace acls are combined, namespace acls can not override what we define in the root
            foreach (AclEntry acl in _authSettings.CurrentValue.Acls)
            {
                allowedActions.AddRange(acl.Resolve(User));
            }

            return Ok(new ActionsResult {Actions = allowedActions});
        }
    }

    public class ActionsResult
    {
        [JsonProperty (ItemConverterType = typeof(StringEnumConverter))]
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Usage", "CA2227:Collection properties should be read only", Justification = "Used by serialization")]
        public List<AclAction> Actions { get; set; } = new List<AclAction>();
    }
}
