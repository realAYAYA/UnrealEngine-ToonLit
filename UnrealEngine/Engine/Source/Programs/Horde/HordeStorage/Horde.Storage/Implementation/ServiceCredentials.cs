// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using Jupiter.Common.Implementation;
using Jupiter.Implementation;
using Microsoft.Extensions.Options;
using RestSharp.Authenticators;

namespace Horde.Storage.Implementation
{
    public interface IServiceCredentials
    {
        IAuthenticator? GetAuthenticator();

        string? GetToken();

        string GetAuthenticationScheme();
    }

    public class ServiceCredentials : IServiceCredentials
    {
        private readonly ClientCredentialOAuthAuthenticator? _authenticator;
        private readonly IOptionsMonitor<ServiceCredentialSettings> _settings;

        public ServiceCredentials(IOptionsMonitor<ServiceCredentialSettings> settings, ISecretResolver secretResolver)
        {
            _settings = settings;
            if (settings.CurrentValue.OAuthLoginUrl != null)
            {
                string? clientId = secretResolver.Resolve(settings.CurrentValue.OAuthClientId);
                if (string.IsNullOrEmpty(clientId))
                {
                    throw new ArgumentException("ClientId must be set when using a service credential");
                }

                string? clientSecret = secretResolver.Resolve(settings.CurrentValue.OAuthClientSecret);
                if (string.IsNullOrEmpty(clientSecret))
                {
                    throw new ArgumentException("ClientSecret must be set when using a service credential");
                }

                _authenticator = new ClientCredentialOAuthAuthenticator(settings.CurrentValue.OAuthLoginUrl, clientId, clientSecret, settings.CurrentValue.OAuthScope, settings.CurrentValue.SchemeName);
            }
        }

        public IAuthenticator? GetAuthenticator()
        {
            return _authenticator;
        }

        public string? GetToken()
        {
            return _authenticator?.Authenticate();
        }

        public string GetAuthenticationScheme()
        {
            return _settings.CurrentValue.SchemeName;
        }
    }
}
