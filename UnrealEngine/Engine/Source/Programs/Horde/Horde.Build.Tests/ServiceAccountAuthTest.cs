// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Linq;
using System.Security.Claims;
using System.Threading.Tasks;
using Horde.Build.Authentication;
using Horde.Build.Users;
using Microsoft.AspNetCore.Authentication;
using Microsoft.AspNetCore.Http;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Primitives;
using Microsoft.Extensions.WebEncoders.Testing;
using Microsoft.Net.Http.Headers;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Build.Tests
{
	[TestClass]
	public class ServiceAccountAuthTest : DatabaseIntegrationTest
	{
		public ServiceAccountAuthTest()
		{
 		}

		protected override void ConfigureServices(IServiceCollection services)
		{
			base.ConfigureServices(services);

			services.AddSingleton<ILoggerFactory, LoggerFactory>();
			services.AddSingleton<ServiceAccountCollection>();
			services.AddSingleton<IServiceAccountCollection>(sp => sp.GetRequiredService<ServiceAccountCollection>());
		}

		private async Task<ServiceAccountAuthHandler> GetAuthHandlerAsync(string? headerValue)
		{
			ServiceAccountAuthOptions options = new ServiceAccountAuthOptions();

			ServiceAccountCollection serviceAccounts = ServiceProvider.GetRequiredService<ServiceAccountCollection>();
			await serviceAccounts.AddAsync("mytoken", new List<string> { "myclaim###myvalue", "foo###bar" }, "mydesc");

			ILoggerFactory loggerFactory = ServiceProvider.GetRequiredService<ILoggerFactory>();
			ServiceAccountAuthHandler handler = new ServiceAccountAuthHandler(new TestOptionsMonitor<ServiceAccountAuthOptions>(options), loggerFactory, new UrlTestEncoder(), new SystemClock(), serviceAccounts);
			AuthenticationScheme scheme = new AuthenticationScheme(ServiceAccountAuthHandler.AuthenticationScheme, "ServiceAccountAuth", handler.GetType());
			
			HttpContext httpContext = new DefaultHttpContext();
			if (headerValue != null)
			{
				httpContext.Request.Headers.Add(HeaderNames.Authorization, new StringValues(headerValue));	
			}
			
			await handler.InitializeAsync(scheme, httpContext);

			return handler;
		}

		[TestMethod]
		public async Task ValidToken()
		{
			ServiceAccountAuthHandler handler = await GetAuthHandlerAsync("ServiceAccount mytoken");
			AuthenticateResult result = await handler.AuthenticateAsync();
			Assert.IsTrue(result.Succeeded);
			
			handler = await GetAuthHandlerAsync("ServiceAccount   mytoken        ");
			result = await handler.AuthenticateAsync();
			Assert.IsTrue(result.Succeeded);
		}
		
		[TestMethod]
		public async Task InvalidToken()
		{
			ServiceAccountAuthHandler handler = await GetAuthHandlerAsync("ServiceAccount doesNotExist");
			AuthenticateResult result = await handler.AuthenticateAsync();
			Assert.IsFalse(result.Succeeded);
			
			// Valid token but bad prefix
			handler = await GetAuthHandlerAsync("SriceAcct mytoken");
			result = await handler.AuthenticateAsync();
			Assert.IsFalse(result.Succeeded);
		}
		
		[TestMethod]
		public async Task NoResult()
		{
			// Valid token but bad prefix
			ServiceAccountAuthHandler handler = await GetAuthHandlerAsync("Bogus mytoken");
			AuthenticateResult result = await handler.AuthenticateAsync();
			Assert.IsFalse(result.Succeeded);
			Assert.IsTrue(result.None);
			
			// No Authorization header at all
			handler = await GetAuthHandlerAsync(null);
			result = await handler.AuthenticateAsync();
			Assert.IsFalse(result.Succeeded);
			Assert.IsTrue(result.None);
		}
		
		[TestMethod]
		public async Task Claims()
		{
			ServiceAccountAuthHandler handler = await GetAuthHandlerAsync("ServiceAccount mytoken");
			AuthenticateResult result = await handler.AuthenticateAsync();
			Assert.IsTrue(result.Succeeded);
			Assert.AreEqual(3, result.Ticket!.Principal.Claims.Count());
			Assert.AreEqual(ServiceAccountAuthHandler.AuthenticationScheme, result.Ticket.Principal.FindFirst(ClaimTypes.Name)!.Value);
			Assert.AreEqual("myvalue", result.Ticket!.Principal.FindFirst("myclaim")!.Value);
			Assert.AreEqual("bar", result.Ticket!.Principal.FindFirst("foo")!.Value);
		}
	}
}