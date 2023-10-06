// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;

#pragma warning disable CS1591 // Missing XML documentation on public types

namespace EpicGames.OIDC
{
	/// <summary>
	/// A generic token store that saves the offline token in a file on disk, this works on any platform
	/// </summary>
	public class FilesystemTokenStore : ITokenStore, IDisposable
	{
		private class TokenStoreState
		{
			public Dictionary<string, string> Providers { get; set; } = new Dictionary<string, string>();
			public string Key { get; set; }

			[JsonConstructor]
			public TokenStoreState(string key, Dictionary<string, string> providers)
			{
				Key = key;
				Providers = providers;
			}

			public TokenStoreState(byte[] key, Dictionary<string, byte[]> providers)
			{
				Key = Convert.ToBase64String(key);

				foreach (KeyValuePair<string, byte[]> pair in providers)
				{
					Providers[pair.Key] = Convert.ToBase64String(pair.Value);
				}
			}
		}

		private readonly Dictionary<string, byte[]> _providerToRefreshToken = new Dictionary<string, byte[]>();
		private readonly SymmetricAlgorithm _crypt = Aes.Create();

		public FilesystemTokenStore()
		{
			ReadStoreFromDisk();
		}

		private static FileInfo GetStorePath()
		{
			return new FileInfo(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "Epic", "UnrealEngine", "Common", "OidcToken", "tokenStore.dat"));
		}

		private void ReadStoreFromDisk()
		{
			FileInfo fi = GetStorePath();
			if (!fi.Exists)
			{
				// if we have no store on disk then we just initialize it to empty
				return;
			}

			using FileStream fs = fi.OpenRead();
			using TextReader tr = new StreamReader(fs);
			TokenStoreState? state;
			try
			{
				state = JsonSerializer.Deserialize<TokenStoreState>(tr.ReadToEnd());
			}
			catch (JsonException)
			{
				state = null;
			}

			if (state == null)
			{
				// if we fail to deserialize the state just drop it, will mean users will need to login again
				return;
			}

			foreach ((string key, string value) in state.Providers)
			{
				_providerToRefreshToken[key] = Convert.FromBase64String(value);
			}

			_crypt.IV = Convert.FromBase64String(state.Key);
		}

		private void SaveStoreToDisk()
		{
			FileInfo fi = GetStorePath();

			if (!fi.Directory?.Exists ?? false)
			{
				Directory.CreateDirectory(fi.Directory!.FullName);
			}

			string tempFile = Path.GetTempFileName();
			{
				using FileStream fs = new FileStream(tempFile, FileMode.Create, FileAccess.Write);
				using Utf8JsonWriter writer = new Utf8JsonWriter(fs);
				JsonSerializer.Serialize<TokenStoreState>(writer, new TokenStoreState(_crypt.IV, _providerToRefreshToken));
			}

			File.Move(tempFile, fi.FullName, true);
		}

		public bool TryGetRefreshToken(string oidcProvider, out string refreshToken)
		{
			if (!_providerToRefreshToken.TryGetValue(oidcProvider, out byte[]? encryptedToken))
			{
				refreshToken = "";
				return false;
			}

			ICryptoTransform decryptor = _crypt.CreateDecryptor(GetSeed(), _crypt.IV);
			byte[] bytes = decryptor.TransformFinalBlock(encryptedToken, 0, encryptedToken.Length);
			refreshToken = Encoding.Unicode.GetString(bytes);
			return true;
		}

		public void AddRefreshToken(string providerIdentifier, string refreshToken)
		{
			byte[] bytes = Encoding.Unicode.GetBytes(refreshToken);
#pragma warning disable CA5401 // Do not use CreateEncryptor with non-default IV
			ICryptoTransform encryptor = _crypt.CreateEncryptor(GetSeed(), _crypt.IV);
#pragma warning restore CA5401 // Do not use CreateEncryptor with non-default IV

			byte[] encryptedToken = encryptor.TransformFinalBlock(bytes, 0, bytes.Length);
			_providerToRefreshToken[providerIdentifier] = encryptedToken;
		}

		public void Save()
		{
			SaveStoreToDisk();
		}

		static byte[] GetSeed()
		{
			return new byte[]
			{
				0x1e, 0x72, 0x5e, 0xe7, 0x08, 0x9e, 0x29, 0x5e, 0xcb, 0xbe, 0x1b, 0xdf, 0x0e, 0xf9, 0x4a, 0x30, 0xd1,
				0xa9, 0x9b, 0xa2, 0xee, 0x58, 0xc4, 0x8e
			};
		}

		protected virtual void Dispose(bool disposing)
		{
			if (disposing)
			{
				SaveStoreToDisk();
				_crypt.Dispose();
			}
		}

		public void Dispose()
		{
			Dispose(true);
			GC.SuppressFinalize(this);
		}
	}
}