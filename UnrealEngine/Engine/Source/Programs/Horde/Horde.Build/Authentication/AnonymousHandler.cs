// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Security.Claims;
using System.Text.Encodings.Web;
using System.Threading.Tasks;
using Horde.Build.Users;
using Horde.Build.Utilities;
using Microsoft.AspNetCore.Authentication;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Horde.Build.Authentication
{
	class AnonymousAuthenticationOptions : AuthenticationSchemeOptions
	{
		public string? AdminClaimType { get; set; }
		public string? AdminClaimValue { get; set; }
	}

	class AnonymousAuthenticationHandler : AuthenticationHandler<AnonymousAuthenticationOptions>
	{
		public const string AuthenticationScheme = "Anonymous";
		readonly IUserCollection _userCollection;

		public AnonymousAuthenticationHandler(IUserCollection userCollection, IOptionsMonitor<AnonymousAuthenticationOptions> options,
			ILoggerFactory logger, UrlEncoder encoder, ISystemClock clock)
			: base(options, logger, encoder, clock)
		{
			_userCollection = userCollection;
		}

		protected override async Task<AuthenticateResult> HandleAuthenticateAsync()
		{
			List<Claim> claims = new List<Claim>();
			claims.Add(new Claim(ClaimTypes.Name, AuthenticationScheme));

			if (Options.AdminClaimType != null && Options.AdminClaimValue != null)
			{
				claims.Add(new Claim(Options.AdminClaimType, Options.AdminClaimValue));
			}

			IUser user = await _userCollection.FindOrAddUserByLoginAsync("anonymous", "Anonymous", "anonymous@epicgames.com");

			ClaimsIdentity identity = new ClaimsIdentity(claims, Scheme.Name);
			identity.AddClaim(new Claim(HordeClaimTypes.UserId, user.Id.ToString()));

			ClaimsPrincipal principal = new ClaimsPrincipal(identity);
			AuthenticationTicket ticket = new AuthenticationTicket(principal, Scheme.Name);

			return AuthenticateResult.Success(ticket);
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
