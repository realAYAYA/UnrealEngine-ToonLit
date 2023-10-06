// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Security.Claims;
using System.Text.Encodings.Web;
using System.Threading.Tasks;
using Horde.Server.Users;
using Horde.Server.Utilities;
using Microsoft.AspNetCore.Authentication;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Horde.Server.Authentication
{
	class AnonymousAuthenticationOptions : AuthenticationSchemeOptions
	{
	}

	class AnonymousAuthenticationHandler : AuthenticationHandler<AnonymousAuthenticationOptions>
	{
		public const string AuthenticationScheme = "Anonymous";

		readonly IOptionsMonitor<ServerSettings> _settings;

		public AnonymousAuthenticationHandler(IOptionsMonitor<AnonymousAuthenticationOptions> options, IOptionsMonitor<ServerSettings> settings, ILoggerFactory logger, UrlEncoder encoder, ISystemClock clock)
			: base(options, logger, encoder, clock)
		{
			_settings = settings;
		}

		protected override Task<AuthenticateResult> HandleAuthenticateAsync()
		{
			List<Claim> claims = new List<Claim>();
			claims.Add(new Claim(ClaimTypes.Name, AuthenticationScheme));
			claims.Add(new Claim(HordeClaimTypes.UserId, UserId.Anonymous.ToString()));
			claims.Add(new Claim(HordeClaims.AdminClaim.Type, HordeClaims.AdminClaim.Value));

			ClaimsIdentity identity = new ClaimsIdentity(claims, Scheme.Name);

			ClaimsPrincipal principal = new ClaimsPrincipal(identity);
			AuthenticationTicket ticket = new AuthenticationTicket(principal, Scheme.Name);

			return Task.FromResult(AuthenticateResult.Success(ticket));
		}
	}

	static class AnonymousExtensions
	{
		public static AuthenticationBuilder AddAnonymous(this AuthenticationBuilder builder, Action<AnonymousAuthenticationOptions> configure)
		{
			return builder.AddScheme<AnonymousAuthenticationOptions, AnonymousAuthenticationHandler>(AnonymousAuthenticationHandler.AuthenticationScheme, configure);
		}
	}
}
