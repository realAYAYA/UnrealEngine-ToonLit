// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Threading;

#pragma warning disable CS1591 // Missing XML documentation on public types

namespace EpicGames.OIDC
{
	public class WindowsTokenStore : ITokenStore, IDisposable
	{
		private class TokenStoreState
		{
			public Dictionary<string, string> Providers { get; set; } = new Dictionary<string, string>();

			[JsonConstructor]
			public TokenStoreState(Dictionary<string, string> providers)
			{
				Providers = providers;
			}

			public TokenStoreState(Dictionary<string, byte[]> providers)
			{
				foreach (KeyValuePair<string, byte[]> pair in providers)
				{
					Providers[pair.Key] = Convert.ToBase64String(pair.Value);
				}
			}
		}

		private readonly Dictionary<string, byte[]> _providerToRefreshToken = new Dictionary<string, byte[]>();
		private bool _isDirty;

		public WindowsTokenStore()
		{
			ReadStoreFromDisk();
		}

		private static FileInfo GetStorePath()
		{
			return new FileInfo(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "UnrealEngine", "Common", "OidcToken", "oidcTokenStore.dat"));
		}

		private void ReadStoreFromDisk()
		{
			FileInfo fi = GetStorePath();
			if (!fi.Exists)
			{
				// if we have no store on disk then we just initialize it to empty
				return;
			}

			using FileStream fs = fi.Open(FileMode.Open, FileAccess.Read, FileShare.Read);
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
		}

		private void SaveStoreToDisk()
		{
			if (!_isDirty)
			{
				return;
			}

			FileInfo fi = GetStorePath();

			if (!fi.Directory?.Exists ?? false)
			{
				Directory.CreateDirectory(fi.Directory!.FullName);
			}

			string tempFile = Path.GetTempFileName();
			{
				using FileStream fs = new FileStream(tempFile, FileMode.Create, FileAccess.Write);
				using Utf8JsonWriter writer = new Utf8JsonWriter(fs);
				JsonSerializer.Serialize<TokenStoreState>(writer, new TokenStoreState(_providerToRefreshToken));
			}

			{
				using Mutex mutex = new Mutex(false, "oidcTokenStoreDat");

				try
				{
					mutex.WaitOne();
				}
				catch (AbandonedMutexException)
				{

				}
				File.Move(tempFile, fi.FullName, true);

				mutex.ReleaseMutex();
			}
			_isDirty = false;
		}

		public bool TryGetRefreshToken(string oidcProvider, out string refreshToken)
		{
			if (!_providerToRefreshToken.TryGetValue(oidcProvider, out byte[]? encryptedToken))
			{
				refreshToken = "";
				return false;
			}

			try
			{
				byte[] bytes = CryptProtectDataHelper.DoCryptUnprotectData(encryptedToken, $"OidcToken-{oidcProvider}", GetEntropy(oidcProvider));
				refreshToken = Encoding.Unicode.GetString(bytes);

				return true;
			}
			catch (Win32Exception e)
			{
				if (e.NativeErrorCode == 13) // data is invalid
				{
					// unable to decrypt the data, ignore it
					refreshToken = "";
					return false;
				}
				if (e.NativeErrorCode == unchecked((int)0x8009000B)) // key not valid for use in specified state
				{
					// unable to decrypt the data, ignore it
					refreshToken = "";
					return false;
				}
				throw;
			}
		}

		private static byte[] GetEntropy(string oidcProvider)
		{
			byte[] providerBytes = Encoding.UTF8.GetBytes(oidcProvider);

			return providerBytes;
		}

		public void AddRefreshToken(string providerIdentifier, string refreshToken)
		{
			byte[] bytes = Encoding.Unicode.GetBytes(refreshToken);
			byte[] encryptedToken = CryptProtectDataHelper.DoCryptProtectData(bytes, $"OidcToken-{providerIdentifier}", GetEntropy(providerIdentifier));

			_providerToRefreshToken[providerIdentifier] = encryptedToken;

			_isDirty = true;
		}

		public void Save()
		{
			SaveStoreToDisk();
		}

		protected virtual void Dispose(bool disposing)
		{
			if (disposing)
			{
				SaveStoreToDisk();
			}
		}

		public void Dispose()
		{
			Dispose(true);
			GC.SuppressFinalize(this);
		}
	}

#pragma warning disable IDE1006 // Pinvoke code doesnt use the same naming conventions as C#
	static class CryptProtectDataHelper
	{
		
		[StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)]
		private struct DataBlob {
			public int cbData;
			public IntPtr pbData;
		}

		[Flags]
		private enum CryptProtectFlags {
			// for remote-access situations where ui is not an option
			// if UI was specified on protect or unprotect operation, the call
			// will fail and GetLastError() will indicate ERROR_PASSWORD_RESTRICTION
			CryptprotectUiForbidden = 0x1,

			// per machine protected data -- any user on machine where CryptProtectData
			// took place may CryptUnprotectData
			CryptprotectLocalMachine = 0x4,

			// force credential synchronize during CryptProtectData()
			// Synchronize is only operation that occurs during this operation
			CryptprotectCredSync = 0x8,

			// Generate an Audit on protect and unprotect operations
			CryptprotectAudit = 0x10,

			// Protect data with a non-recoverable key
			CryptprotectNoRecovery = 0x20,

			// Verify the protection of a protected blob
			CryptprotectVerifyProtection = 0x40
		}

		[Flags]
		private enum CryptProtectPromptFlags {
			// prompt on unprotect
			CryptprotectPromptOnUnprotect = 0x1,

			// prompt on protect
			CryptprotectPromptOnProtect = 0x2
		}

		[StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)]
		private struct CryptprotectPromptstruct {
			public int cbSize;
			public CryptProtectPromptFlags dwPromptFlags;
			public IntPtr hwndApp;
			public string szPrompt;
		}

		[
			DllImport("Crypt32.dll",
				SetLastError=true,
				CharSet=System.Runtime.InteropServices.CharSet.Auto)
		]
		[return: MarshalAs(UnmanagedType.Bool)]
		private static extern bool CryptProtectData(
			ref DataBlob pDataIn,
			string szDataDescr,
			ref DataBlob pOptionalEntropy,
			IntPtr pvReserved,
			IntPtr pPromptStruct,
			CryptProtectFlags dwFlags,
			ref DataBlob pDataOut
		);

		[
			DllImport("Crypt32.dll",
				SetLastError=true,
				CharSet=System.Runtime.InteropServices.CharSet.Auto)
		]
		[return: MarshalAs(UnmanagedType.Bool)]
		private static extern bool CryptUnprotectData(
			ref DataBlob pDataIn,
			string szDataDescr,
			ref DataBlob pOptionalEntropy,
			IntPtr pvReserved,
			IntPtr pPromptStruct,
			CryptProtectFlags dwFlags,
			ref DataBlob pDataOut
		);

		public static byte[] DoCryptProtectData(byte[] dataToProtect, string description, byte[] entropy)
		{
			DataBlob dataOut = new DataBlob();

			GCHandle dataHandle = GCHandle.Alloc(dataToProtect, GCHandleType.Pinned);
			GCHandle entropyHandle = GCHandle.Alloc(entropy, GCHandleType.Pinned);
			try
			{
				Marshal.Copy(dataToProtect, 0, dataHandle.AddrOfPinnedObject(), dataToProtect.Length);
				Marshal.Copy(entropy, 0, entropyHandle.AddrOfPinnedObject(), entropy.Length);

				DataBlob data = new DataBlob()
				{
					cbData = dataToProtect.Length,
					pbData = dataHandle.AddrOfPinnedObject()
				};

				DataBlob entropyBlob = new DataBlob()
				{
					cbData = entropy.Length,
					pbData = entropyHandle.AddrOfPinnedObject()
				};

				CryptProtectFlags flags = 0;

				if (!CryptProtectData(ref data, description, ref entropyBlob, IntPtr.Zero, IntPtr.Zero, flags, ref dataOut))
				{
					throw new Win32Exception();
				}
			}
			finally
			{
				dataHandle.Free();
				entropyHandle.Free();
			}
		
		
			byte[] buf = new byte[dataOut.cbData];
			Marshal.Copy(dataOut.pbData, buf, 0, dataOut.cbData);
			return buf;
		}

		public static byte[] DoCryptUnprotectData(byte[] dataToDecrypt, string description, byte[] entropy)
		{
			DataBlob dataOut = new DataBlob();

			GCHandle dataHandle = GCHandle.Alloc(dataToDecrypt, GCHandleType.Pinned);
			GCHandle entropyHandle = GCHandle.Alloc(entropy, GCHandleType.Pinned);
			try
			{
				Marshal.Copy(dataToDecrypt, 0, dataHandle.AddrOfPinnedObject(), dataToDecrypt.Length);
				Marshal.Copy(entropy, 0, entropyHandle.AddrOfPinnedObject(), entropy.Length);

				DataBlob data = new DataBlob()
				{
					cbData = dataToDecrypt.Length,
					pbData = dataHandle.AddrOfPinnedObject()
				};

				DataBlob entropyBlob = new DataBlob()
				{
					cbData = entropy.Length,
					pbData = entropyHandle.AddrOfPinnedObject()
				};

				CryptProtectFlags flags = 0;

				if (!CryptUnprotectData(ref data, description, ref entropyBlob, IntPtr.Zero, IntPtr.Zero, flags, ref dataOut))
				{
					throw new Win32Exception();
				}
			}
			finally
			{
				dataHandle.Free();
				entropyHandle.Free();
			}
		
		
			byte[] buf = new byte[dataOut.cbData];
			Marshal.Copy(dataOut.pbData, buf, 0, dataOut.cbData);
			return buf;
		}
	}
#pragma warning restore IDE1006 // Naming Styles
}