// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Security.Claims;
using System.Text.Encodings.Web;
using System.Threading.Tasks;
using Horde.Build.Users;
using Microsoft.AspNetCore.Authentication;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using Microsoft.Net.Http.Headers;

namespace Horde.Build.Authentication
{
	class ServiceAccountAuthOptions : AuthenticationSchemeOptions
	{
	}

	class ServiceAccountAuthHandler : AuthenticationHandler<ServiceAccountAuthOptions>
	{
		public const string AuthenticationScheme = "ServiceAccount";
		public const string Prefix = "ServiceAccount";

		private readonly IServiceAccountCollection _serviceAccounts;

		public ServiceAccountAuthHandler(IOptionsMonitor<ServiceAccountAuthOptions> options,
			ILoggerFactory logger, UrlEncoder encoder, ISystemClock clock, IServiceAccountCollection serviceAccounts)
			: base(options, logger, encoder, clock)
		{
			_serviceAccounts = serviceAccounts;
		}
		
		protected override async Task<AuthenticateResult> HandleAuthenticateAsync()
		{
			if (!Context.Request.Headers.TryGetValue(HeaderNames.Authorization, out Microsoft.Extensions.Primitives.StringValues headerValue))
			{
				return AuthenticateResult.NoResult();
			}

			if (headerValue.Count < 1)
			{
				return AuthenticateResult.NoResult();
			}

			if (!headerValue[0].StartsWith(Prefix, StringComparison.Ordinal))
			{
				return AuthenticateResult.NoResult();
			}
			
			string token = headerValue[0].Replace(Prefix, "", StringComparison.Ordinal).Trim();
			IServiceAccount? serviceAccount = await _serviceAccounts.GetBySecretTokenAsync(token);

			if (serviceAccount == null)
			{
				return AuthenticateResult.Fail($"Service account for token {token} not found");
			}

			List<Claim> claims = new List<Claim>(10);
			claims.Add(new Claim(ClaimTypes.Name, AuthenticationScheme));
			claims.AddRange(serviceAccount.GetClaims().Select(claimPair => new Claim(claimPair.Type, claimPair.Value)));

			ClaimsIdentity identity = new ClaimsIdentity(claims, Scheme.Name);
			ClaimsPrincipal principal = new ClaimsPrincipal(identity);
			AuthenticationTicket ticket = new AuthenticationTicket(principal, Scheme.Name);

			return AuthenticateResult.Success(ticket);
		}
	}

	static class ServiceAccountExtensions
	{
		public static AuthenticationBuilder AddServiceAccount(this AuthenticationBuilder builder, Action<ServiceAccountAuthOptions> config)
		{
			return builder.AddScheme<ServiceAccountAuthOptions, ServiceAccountAuthHandler>(ServiceAccountAuthHandler.AuthenticationScheme, config);
		}
	}
}
