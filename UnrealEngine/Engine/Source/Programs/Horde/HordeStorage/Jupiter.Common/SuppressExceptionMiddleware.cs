// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading.Tasks;
using EpicGames.AspNet;
using Microsoft.AspNetCore.Http;
using Microsoft.Extensions.DependencyInjection;
using Serilog;

namespace Jupiter.Common
{
    public class SuppressExceptionMiddleware
	{
        private readonly ILogger _logger = Log.ForContext<SuppressExceptionMiddleware>();
		private readonly RequestDelegate _next;

        public SuppressExceptionMiddleware(RequestDelegate next)
        {
            _next = next ?? throw new ArgumentNullException(nameof(next));
        }

        public async Task Invoke(HttpContext context)
        {
            try
            {
                await _next(context);
            }
            // suppress certain known exceptions that are not considered errors but that we can not catch in our code
            catch (ClientSendSlowException e)
            {
                // always suppress slow sends
                _logger.Warning(e, "Client was sending data slowly");
            }
            catch (TaskCanceledException e)
            {
                // if health checks are cancelled its fine, it means it was called while it was running
                string pathString = context.Request.Path.Value ?? "";

                if (!pathString.StartsWith("/health", StringComparison.OrdinalIgnoreCase))
                {
                    // rethrow
                    throw;
                }
                else
                {
                    _logger.Warning(e, "Health check was cancelled. This can happen if multiple health checks happen at the same time");
                }
            }
        }
    }

	public static class SuppressExceptionMiddlewareCollectionExtensions
	{
        public static IServiceCollection AddSuppressExceptionsMiddleware(this IServiceCollection services)
        {
            services.AddSingleton<SuppressExceptionMiddleware>();

            return services;
        }
    }
}
