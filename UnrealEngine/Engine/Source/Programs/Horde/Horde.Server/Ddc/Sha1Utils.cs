// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Security.Cryptography;
using System.Threading.Tasks;

#pragma warning disable CS1591
#pragma warning disable CA5350 // Do not use weak crypto algorithms

namespace Horde.Server.Ddc
{
	public static class Sha1Utils
	{
		public static async Task<string> GetSHA1AsStringAsync(byte[] payload)
		{
			return await Task.Run(() => GetSHA1AsString(payload));
		}

		public static byte[] GetSHA1(byte[] payload)
		{
			using SHA1 sha1 = SHA1.Create();
			return sha1.ComputeHash(payload);
		}

		public static byte[] GetSHA1(byte[] payload, int offset, int count)
		{
			using SHA1 sha1 = SHA1.Create();
			return sha1.ComputeHash(payload, offset, count);
		}

		public static string GetSHA1AsString(byte[] payload)
		{
			byte[] sha1 = GetSHA1(payload);

			return StringUtils.FormatAsHexString(sha1);
		}
	}

	public class ByteArrayComparer : EqualityComparer<byte[]>
	{
		public override bool Equals(byte[]? a, byte[]? b)
		{
			if (a == null)
			{
				return b == null;
			}

			if (b == null)
			{
				return false;
			}

			if (a.Length != b.Length)
			{
				return false;
			}

			for (int i = 0; i < a.Length; i++)
			{
				if (a[i] != b[i])
				{
					return false;
				}
			}
			return true;
		}
		public override int GetHashCode(byte[] a)
		{
			uint b = 0;
			foreach (byte t in a)
			{
				b = ((b << 23) | (b >> 9)) ^ t;
			}

			return unchecked((int)b);
		}
	}
}
