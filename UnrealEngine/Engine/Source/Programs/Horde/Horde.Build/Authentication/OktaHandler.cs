// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Linq;
using System.Security.Claims;
using System.Text.Encodings.Web;
using System.Text.Json;
using System.Threading.Tasks;
using Horde.Build.Users;
using Horde.Build.Utilities;
using Microsoft.AspNetCore.Authentication;
using Microsoft.AspNetCore.Authentication.OAuth.Claims;
using Microsoft.AspNetCore.Authentication.OpenIdConnect;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.DependencyInjection.Extensions;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using Microsoft.IdentityModel.Protocols.OpenIdConnect;

namespace Horde.Build.Authentication
{
	class OktaDefaults
	{
		public const string AuthenticationScheme = "Okta" + OpenIdConnectDefaults.AuthenticationScheme;
	}

	class OktaHandler : OpenIdConnectHandler
	{
		readonly IUserCollection _userCollection;

		public OktaHandler(IOptionsMonitor<OpenIdConnectOptions> options, ILoggerFactory logger, HtmlEncoder htmlEncoder, UrlEncoder encoder, ISystemClock clock, IUserCollection userCollection)
			: base(options, logger, htmlEncoder, encoder, clock)
		{
			_userCollection = userCollection;
		}

		public static void AddUserInfoClaims(JsonElement userInfo, ClaimsIdentity identity)
		{
			JsonElement userElement;
			if (userInfo.TryGetProperty("preferred_username", out userElement))
			{
				if (identity.FindFirst(ClaimTypes.Name) == null)
				{
					identity.AddClaim(new Claim(ClaimTypes.Name, userElement.ToString()!));
				}
				if (identity.FindFirst(HordeClaimTypes.User) == null)
				{
					identity.AddClaim(new Claim(HordeClaimTypes.User, userElement.ToString()!));
				}
				if (identity.FindFirst(HordeClaimTypes.PerforceUser) == null)
				{
					identity.AddClaim(new Claim(HordeClaimTypes.PerforceUser, userElement.ToString()!));
				}
			}

			JsonElement groupsElement;
			if (userInfo.TryGetProperty("groups", out groupsElement) && groupsElement.ValueKind == JsonValueKind.Array)
			{
				for (int idx = 0; idx < groupsElement.GetArrayLength(); idx++)
				{
					identity.AddClaim(new Claim(ClaimTypes.Role, groupsElement[idx].ToString()!));
				}
			}

			JsonElement emailElement;
			if (userInfo.TryGetProperty("email", out emailElement) && identity.FindFirst(ClaimTypes.Email) == null)
			{
				identity.AddClaim(new Claim(ClaimTypes.Email, emailElement.ToString()!));
			}
		}

		protected override async Task<HandleRequestResult> HandleRemoteAuthenticateAsync()
		{
			// Authenticate with the OIDC provider
			HandleRequestResult result = await base.HandleRemoteAuthenticateAsync();
			if (!result.Succeeded)
			{
				return result;
			}

			ClaimsIdentity? identity = (ClaimsIdentity?)result.Principal?.Identity;
			if (identity == null)
			{
				return HandleRequestResult.Fail("No identity specified");
			}

			string login = identity.FindFirst(HordeClaimTypes.User)!.Value;
			string? name = identity.FindFirst(ClaimTypes.Name)?.Value;
			string? email = identity.FindFirst(ClaimTypes.Email)?.Value;

			IUser user = await _userCollection.FindOrAddUserByLoginAsync(login, name, email);
			identity.AddClaim(new Claim(HordeClaimTypes.UserId, user.Id.ToString()));

			await _userCollection.UpdateClaimsAsync(user.Id, identity.Claims.Select(x => new UserClaim(x.Type, x.Value)));

			return result;
		}
	}

	static class OktaExtensions
	{
		class MapRolesClaimAction : ClaimAction
		{
			public MapRolesClaimAction()
				: base(ClaimTypes.Role, ClaimTypes.Role)
			{
			}

			public override void Run(JsonElement userData, ClaimsIdentity identity, string issuer)
			{
				OktaHandler.AddUserInfoClaims(userData, identity);
			}
		}

		static void ApplyDefaultOktaOptions(OpenIdConnectOptions options, Action<OpenIdConnectOptions> handler)
		{
			options.Scope.Add("profile");
			options.Scope.Add("groups");
			options.Scope.Add("email");
			options.ResponseType = OpenIdConnectResponseType.Code;
			options.GetClaimsFromUserInfoEndpoint = true;
			options.SaveTokens = true;
			options.TokenValidationParameters.NameClaimType = "name";
			options.ClaimActions.Add(new MapRolesClaimAction());

			handler(options);
		}

		public static void AddOkta(this AuthenticationBuilder builder, string authenticationScheme, string displayName, Action<OpenIdConnectOptions> handler)
		{
			builder.Services.TryAddEnumerable(ServiceDescriptor.Singleton<IPostConfigureOptions<OpenIdConnectOptions>, OpenIdConnectPostConfigureOptions>());
			builder.AddRemoteScheme<OpenIdConnectOptions, OktaHandler>(authenticationScheme, displayName, options => ApplyDefaultOktaOptions(options, handler));
		}
	}
}
