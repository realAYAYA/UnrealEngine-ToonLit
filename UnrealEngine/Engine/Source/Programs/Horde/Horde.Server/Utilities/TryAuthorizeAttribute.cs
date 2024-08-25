// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading.Tasks;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Authorization.Policy;
using Microsoft.AspNetCore.Mvc.Filters;
using Microsoft.Extensions.DependencyInjection;

namespace Horde.Server.Utilities
{
	/// <summary>
	/// Attempts to authorize the user, but does not challenge or forbid access if authorization fails.
	/// </summary>
	[AttributeUsage(AttributeTargets.Method | AttributeTargets.Class)]
	sealed class TryAuthorizeAttribute : Attribute, IAsyncAuthorizationFilter
	{
		/// <inheritdoc/>
		public async Task OnAuthorizationAsync(AuthorizationFilterContext context)
		{
			IAuthorizationPolicyProvider policyProvider = context.HttpContext.RequestServices.GetRequiredService<IAuthorizationPolicyProvider>();
			AuthorizationPolicy effectivePolicy = await policyProvider.GetDefaultPolicyAsync();

			IPolicyEvaluator policyEvaluator = context.HttpContext.RequestServices.GetRequiredService<IPolicyEvaluator>();
			await policyEvaluator.AuthenticateAsync(effectivePolicy, context.HttpContext);
		}
	}
}
