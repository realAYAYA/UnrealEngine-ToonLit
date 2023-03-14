// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Net;
using RestSharp;
using RestSharp.Authenticators;

namespace Jupiter.Implementation
{
    public class ClientCredentialOAuthAuthenticator: IAuthenticator
    {
        public ClientCredentialOAuthAuthenticator(Uri authUrl, string clientId, string clientSecret, string scope, string schemeName)
        {
            _authUrl = authUrl;
            _clientId = clientId;
            _clientSecret = clientSecret;
            _scope = scope;
            _schemeName = schemeName;
        }

        private string? _accessToken;

        private readonly Uri _authUrl;
        private readonly string _clientId;
        private readonly string _clientSecret;
        private readonly string _scope;
        private readonly string _schemeName;
        private DateTime _expiresAt;

        public void Authenticate(IRestClient client, IRestRequest request)
        {
            if (string.IsNullOrEmpty(_accessToken) || DateTime.Now > _expiresAt)
            {
                PreAuthenticate();
            }

            request.AddHeader("Authorization", $"{_schemeName} {_accessToken}");
        }

        public string? Authenticate()
        {
            if (string.IsNullOrEmpty(_accessToken) || DateTime.Now > _expiresAt)
            {
                PreAuthenticate();
            }

            return _accessToken;
        }

        private void PreAuthenticate()
        {
            IRestResponse<ClientCredentialsResponse> authRequest = DoAuthenticationRequest();
            try
            {
                if (!authRequest.IsSuccessful)
                {
                    throw new AuthenticationFailedException(authRequest.Content);
                }

                ClientCredentialsResponse result = authRequest.Data;
                string? accessToken = result?.access_token;
                if (string.IsNullOrEmpty(accessToken))
                {
                    throw new InvalidOperationException("The authentication token received by the server is null or empty. Body received was: " + authRequest.Content);
                }
                _accessToken = accessToken;
                // renew after half the renewal time
                _expiresAt = DateTime.Now + TimeSpan.FromSeconds((result?.expires_in ?? 3200) / 2.0);
            }
            catch (WebException ex)
            {
                if (ex.Response is HttpWebResponse response && response.StatusCode != HttpStatusCode.InternalServerError)
                {
                    string errorResult = authRequest.Content;
                    if (errorResult == null)
                    {
                        throw;
                    }
                    throw new AuthenticationFailedException(errorResult);
                }
                throw;
            }
        }

        private IRestResponse<ClientCredentialsResponse> DoAuthenticationRequest()
        {
            RestClient client = new RestClient(_authUrl);
            RestRequest request = new RestRequest(Method.POST);

            request.AddParameter("grant_type", "client_credentials", ParameterType.GetOrPost);
            request.AddParameter("client_id", _clientId, ParameterType.GetOrPost);
            request.AddParameter("client_secret", _clientSecret, ParameterType.GetOrPost);
            request.AddParameter("scope", _scope, ParameterType.GetOrPost);
            IRestResponse<ClientCredentialsResponse> response = client.Execute<ClientCredentialsResponse>(request);

            return response;
        }
    }

    // ReSharper disable once ClassNeverInstantiated.Global
    internal class ClientCredentialsResponse
    {
        public string? access_token { get; set; }

        public string? token_type { get; set; }
        
        public int? expires_in { get; set; }

        public string? scope { get; set; }
    }

    public class AuthenticationFailedException : Exception
    {
        public AuthenticationFailedException(object errorResult) : base(errorResult.ToString()) { }
    }
}
