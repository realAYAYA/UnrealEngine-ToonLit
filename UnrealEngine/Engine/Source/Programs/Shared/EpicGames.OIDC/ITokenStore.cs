// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Diagnostics.CodeAnalysis;
using System.Runtime.InteropServices;
using Microsoft.Extensions.DependencyInjection;

#pragma warning disable CS1591 // Missing XML documentation on public types

namespace EpicGames.OIDC
{
	public interface ITokenStore: IDisposable
	{
		public bool TryGetRefreshToken(string oidcProvider, [MaybeNullWhen(false)] out string refreshToken);

		public void AddRefreshToken(string providerIdentifier, string refreshToken);
		public void Save();
	}

	public static class TokenStoreFactory
	{
		public static ITokenStore CreateTokenStore(IServiceProvider provider)
		{
			if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				return ActivatorUtilities.CreateInstance<WindowsTokenStore>(provider);
			}
			else if (RuntimeInformation.IsOSPlatform(OSPlatform.OSX))
			{
				return ActivatorUtilities.CreateInstance<MacOSTokenStore>(provider);
			}
			else if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
			{
				return ActivatorUtilities.CreateInstance<FilesystemTokenStore>(provider);
			}

			throw new NotSupportedException("Unknown platform when attempting to create ITokenStore");
		}

		public static ITokenStore CreateTokenStore()
		{
			if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				return new WindowsTokenStore();
			}
			else if (RuntimeInformation.IsOSPlatform(OSPlatform.OSX))
			{
				return new MacOSTokenStore();
			}
			else if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
			{
				return new FilesystemTokenStore();
			}

			throw new NotSupportedException("Unknown platform when attempting to create ITokenStore");
		}
	}
}