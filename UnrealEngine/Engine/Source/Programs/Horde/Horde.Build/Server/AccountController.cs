// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Globalization;
using System.Net;
using System.Text;
using System.Threading.Tasks;
using Horde.Build.Acls;
using Horde.Build.Authentication;
using Microsoft.AspNetCore.Authentication;
using Microsoft.AspNetCore.Authentication.Cookies;
using Microsoft.AspNetCore.Authentication.OpenIdConnect;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Options;

namespace Horde.Build.Server
{
	/// <summary>
	/// Controller managing account status
	/// </summary>
	[ApiController]
	[Route("[controller]")]
	public class AccountController : Controller
	{
		/// <summary>
		/// Style sheet for HTML responses
		/// </summary>
		const string StyleSheet =
			"body { font-family: 'Segoe UI', 'Roboto', arial, sans-serif; } " +
			"p { margin:20px; font-size:13px; } " +
			"h1 { margin:20px; font-size:32px; font-weight:200; } " +
			"table { margin:10px 20px; } " +
			"td { margin:5px; font-size:13px; }";

		/// <summary>
		/// The ACL service singleton
		/// </summary>
		readonly AclService _aclService;

		/// <summary>
		/// Authentication scheme in use
		/// </summary>
		readonly string _authenticationScheme;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="aclService">ACL service instance</param>
		/// <param name="serverSettings">Server settings</param>
		public AccountController(AclService aclService, IOptionsMonitor<ServerSettings> serverSettings)
		{
			_aclService = aclService;
			_authenticationScheme = GetAuthScheme(serverSettings.CurrentValue.AuthMethod);
		}

		/// <summary>
		/// Get auth scheme name for a given auth method
		/// </summary>
		/// <param name="method">Authentication method</param>
		/// <returns>Name of authentication scheme</returns>
		public static string GetAuthScheme(AuthMethod method)
		{
			return method switch
			{
				AuthMethod.Anonymous => AnonymousAuthenticationHandler.AuthenticationScheme,
				AuthMethod.Okta => OktaDefaults.AuthenticationScheme,
				AuthMethod.OpenIdConnect => OpenIdConnectDefaults.AuthenticationScheme,
				_ => throw new ArgumentOutOfRangeException(nameof(method), method, null)
			};
		}

		/// <summary>
		/// Gets the current login status
		/// </summary>
		/// <returns>The current login state</returns>
		[HttpGet]
		[Route("/account")]
		public async Task<ActionResult> State()
		{
			StringBuilder content = new StringBuilder();
			content.Append($"<html><style>{StyleSheet}</style><h1>Horde Server</h1>");
			if (User.Identity?.IsAuthenticated ?? false)
			{
				content.Append(CultureInfo.InvariantCulture, $"<p>User <b>{User.Identity?.Name}</b> is logged in. <a href=\"/account/logout\">Log out</a></p>");
				if (await _aclService.AuthorizeAsync(AclAction.AdminWrite, User))
				{
					content.Append("<p>");
					content.Append("<a href=\"/api/v1/admin/token\">Get bearer token</a><br/>");
					content.Append("<a href=\"/api/v1/admin/registrationtoken\">Get agent registration token</a><br/>");
					content.Append("<a href=\"/api/v1/admin/softwaretoken\">Get agent software upload token</a><br/>");
					content.Append("<a href=\"/api/v1/admin/softwaredownloadtoken\">Get agent software download token</a><br/>");
					content.Append("<a href=\"/api/v1/admin/configtoken\">Get configuration token</a><br/>");
					content.Append("<a href=\"/api/v1/admin/chainedjobtoken\">Get chained job token</a><br/>");
					content.Append("</p>");
				}
				content.Append(CultureInfo.InvariantCulture, $"<p>Claims for {User.Identity?.Name}:");
				content.Append("<table>");
				foreach (System.Security.Claims.Claim claim in User.Claims)
				{
					content.Append(CultureInfo.InvariantCulture, $"<tr><td>{claim.Type}</td><td>{claim.Value}</td></tr>");
				}
				content.Append("</table>");
				content.Append("</p>");

				content.Append(CultureInfo.InvariantCulture, $"<p>Built from Perforce</p>");
			}
			else
			{
				content.Append("<p><a href=\"/account/login\"><b>Login with OAuth2</b></a></p>");
			}
			content.Append("</html>");
			return new ContentResult { ContentType = "text/html", StatusCode = (int)HttpStatusCode.OK, Content = content.ToString() };
		}

		/// <summary>
		/// Login to the server
		/// </summary>
		/// <returns>Http result</returns>
		[HttpGet]
		[Route("/account/login")]
		public IActionResult Login()
		{
			return new ChallengeResult(_authenticationScheme, new AuthenticationProperties { RedirectUri = "/account" });
		}

		/// <summary>
		/// Logout of the current account
		/// </summary>
		/// <returns>Http result</returns>
		[HttpGet]
		[Route("/account/logout")]
		public async Task<IActionResult> Logout()
		{
			await HttpContext.SignOutAsync(CookieAuthenticationDefaults.AuthenticationScheme);
			try
			{
				await HttpContext.SignOutAsync(_authenticationScheme);
			}
			catch
			{
			}

			string content = $"<html><style>{StyleSheet}</style><body onload=\"setTimeout(function(){{ window.location = '/account'; }}, 2000)\"><p>User has been logged out. Returning to login page.</p></body></html>";
			return new ContentResult { ContentType = "text/html", StatusCode = (int)HttpStatusCode.OK, Content = content };
		}
	}
}
