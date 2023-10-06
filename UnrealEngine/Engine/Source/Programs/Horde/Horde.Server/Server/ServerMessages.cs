// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Diagnostics;
using System.Reflection;
using System.Runtime.InteropServices;

namespace Horde.Server.Server
{
	/// <summary>
	/// Server Info
	/// </summary>
	public class GetServerInfoResponse
	{
        /// <summary>
		/// Server version info
		/// </summary>
        public string ServerVersion { get; set; }

		/// <summary>
		/// The current agent version string
		/// </summary>
		public string? AgentVersion { get; set; }

        /// <summary>
        /// The operating system server is hosted on
        /// </summary>
        public string OsDescription { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		public GetServerInfoResponse(string? agentVersion)
        {
            FileVersionInfo versionInfo = FileVersionInfo.GetVersionInfo(Assembly.GetExecutingAssembly().Location);		
			ServerVersion = versionInfo.ProductVersion ?? String.Empty;
			AgentVersion = agentVersion;
			OsDescription = RuntimeInformation.OSDescription;			
		}
	}

	/// <summary>
	/// Gets connection information to the server
	/// </summary>
	public class GetConnectionResponse
	{
		/// <summary>
		/// Public IP address of the remote machine
		/// </summary>
		public string? Ip { get; set; }

		/// <summary>
		/// Public port of the connecting machine
		/// </summary>
		public int Port { get; set; }
	}

	/// <summary>
	/// Gets ports configured for this server
	/// </summary>
	public class GetPortsResponse
	{
		/// <summary>
		/// Port for HTTP communication
		/// </summary>
		public int? Http { get; set; }

		/// <summary>
		/// Port number for HTTPS communication
		/// </summary>
		public int? Https { get; set; }

		/// <summary>
		/// Port number for unencrpyted HTTPS communication
		/// </summary>
		public int? UnencryptedHttp2 { get; set; }
	}

	/// <summary>
	/// Describes the auth config for this server
	/// </summary>
	public class GetAuthConfigResponse
	{
		/// <inheritdoc cref="ServerSettings.AuthMethod"/>
		public AuthMethod Method { get; }

		/// <inheritdoc cref="ServerSettings.OidcAuthority"/>
		public string? ServerUrl { get; }

		/// <inheritdoc cref="ServerSettings.OidcClientId"/>
		public string? ClientId { get; }

		/// <inheritdoc cref="ServerSettings.OidcLocalRedirectUrls"/>
		public string[]? LocalRedirectUrls { get; }

		internal GetAuthConfigResponse(ServerSettings settings)
		{
			Method = settings.AuthMethod;
			ServerUrl = settings.OidcAuthority;
			ClientId = settings.OidcClientId;
			LocalRedirectUrls = settings.OidcLocalRedirectUrls;
		}
	}
}

