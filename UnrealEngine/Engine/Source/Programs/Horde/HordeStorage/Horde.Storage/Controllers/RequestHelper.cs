// Copyright Epic Games, Inc. All Rights Reserved.

using System.Security.Claims;
using System.Threading.Tasks;
using Datadog.Trace;
using EpicGames.Horde.Storage;
using Jupiter;
using Jupiter.Common;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Options;

namespace Horde.Storage.Controllers;

public class RequestHelper
{
    private readonly IAuthorizationService _authorizationService;
    private readonly INamespacePolicyResolver _namespacePolicyResolver;
    private readonly IOptionsMonitor<JupiterSettings> _settings;

    public RequestHelper(IAuthorizationService authorizationService, INamespacePolicyResolver namespacePolicyResolver, IOptionsMonitor<JupiterSettings> settings)
    {
        _authorizationService = authorizationService;
        _namespacePolicyResolver = namespacePolicyResolver;
        _settings = settings;
    }

    public async Task<ActionResult?> HasAccessToNamespace(ClaimsPrincipal user, HttpRequest request, NamespaceId ns, AclAction[] aclActions)
    {
        using IScope _ = Tracer.Instance.StartActive("authorize");
        AuthorizationResult authorizationResult = await _authorizationService.AuthorizeAsync(user, new NamespaceAccessRequest
        {
            Namespace = ns,
            Actions = aclActions
        }, NamespaceAccessRequirement.Name);

        if (!authorizationResult.Succeeded)
        {
            return new ForbidResult();
        }

        bool isPublicNamespace = _namespacePolicyResolver.GetPoliciesForNs(ns).IsPublicNamespace;

        // public namespaces are always accessible
        if (isPublicNamespace)
        {
            return null;
        }

        // namespace is a restricted namespace
        HttpContext context = request.HttpContext;
        bool isUnitTest = context.Connection.LocalPort == 0 && context.Connection.LocalIpAddress == null;
        /* unit tests do not run on ports, we consider them always on the internal port */
        bool isPublicPort = _settings!.CurrentValue.PublicApiPorts.Contains(context.Connection.LocalPort);
            
        if (isPublicPort && !isUnitTest)
        {
            // trying to access restricted namespace on a public port, this is not allowed
            return new ForbidResult();
        }

        // restricted namespace in corp or internal port, this is okay
        return null;
    }

    public async Task<ActionResult?> HasAccessForGlobalOperations(ClaimsPrincipal user, AclAction[] aclActions)
    {
        using IScope _ = Tracer.Instance.StartActive("authorize");
        AuthorizationResult authorizationResult = await _authorizationService.AuthorizeAsync(user, new GlobalAccessRequest
        {
            Actions = aclActions
        }, GlobalAccessRequirement.Name);

        if (!authorizationResult.Succeeded)
        {
            return new ForbidResult();
        }

        return null;
    }
}
