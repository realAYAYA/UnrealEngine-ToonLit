// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IdentityModel.Tokens.Jwt;
using System.Text.Encodings.Web;
using System.Threading.Tasks;
using Horde.Build.Server;
using Horde.Build.Utilities;
using Microsoft.AspNetCore.Authentication;
using Microsoft.AspNetCore.Authentication.JwtBearer;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Horde.Build.Authentication
{
	/// <summary>
	/// JWT handler for server-issued bearer tokens. These tokens are signed using a randomly generated key per DB instance.
	/// </summary>
	class HordeJwtBearerHandler : JwtBearerHandler
	{
		/// <summary>
		/// Default name of the authentication scheme
		/// </summary>
		public const string AuthenticationScheme = "ServerJwt";
		readonly MongoService _mongoService;

		public HordeJwtBearerHandler(ILoggerFactory logger, UrlEncoder encoder, ISystemClock clock, MongoService mongoService, IOptionsMonitorCache<JwtBearerOptions> optionsCache)
			: base(GetOptionsMonitor(mongoService, optionsCache), logger, encoder, clock)
		{
			_mongoService = mongoService;
		}

		private static IOptionsMonitor<JwtBearerOptions> GetOptionsMonitor(MongoService mongoService, IOptionsMonitorCache<JwtBearerOptions> optionsCache)
		{
			ConfigureNamedOptions<JwtBearerOptions> namedOptions = new ConfigureNamedOptions<JwtBearerOptions>(AuthenticationScheme, options => Configure(options, mongoService));
			OptionsFactory<JwtBearerOptions> optionsFactory = new OptionsFactory<JwtBearerOptions>(new[] { namedOptions }, Array.Empty<IPostConfigureOptions<JwtBearerOptions>>());
			return new OptionsMonitor<JwtBearerOptions>(optionsFactory, Array.Empty<IOptionsChangeTokenSource<JwtBearerOptions>>(), optionsCache);
		}

		private static void Configure(JwtBearerOptions options, MongoService mongoService)
		{
			options.TokenValidationParameters.ValidateAudience = false;

			options.TokenValidationParameters.RequireExpirationTime = false;
			options.TokenValidationParameters.ValidateLifetime = true;

			options.TokenValidationParameters.ValidIssuer = mongoService.JwtIssuer;
			options.TokenValidationParameters.ValidateIssuer = true;

			options.TokenValidationParameters.ValidateIssuerSigningKey = true;
			options.TokenValidationParameters.IssuerSigningKey = mongoService.JwtSigningKey;
		}

		protected override Task<AuthenticateResult> HandleAuthenticateAsync()
		{
			// Silent fail if this JWT is not issued by the server
			string? token;
			if (!JwtUtils.TryGetBearerToken(Request, "Bearer ", out token))
			{
				return Task.FromResult(AuthenticateResult.NoResult());
			}

			// Validate that it's from the correct issuer
			JwtSecurityToken? jwtToken;
			if (!JwtUtils.TryParseJwt(token, out jwtToken) || !String.Equals(jwtToken.Issuer, _mongoService.JwtIssuer, StringComparison.Ordinal))
			{
				return Task.FromResult(AuthenticateResult.NoResult());
			}

			// Pass it to the base class
			return base.HandleAuthenticateAsync();
		}
	}
}
