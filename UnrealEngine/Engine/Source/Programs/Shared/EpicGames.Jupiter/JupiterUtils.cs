// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using System;
using System.Collections.Generic;
using System.IO;
using System.Security.Cryptography;
using System.Text;

namespace EpicGames.Jupiter
{

	public static class JupiterFileUtils
	{
		public static List<Tuple<byte[], byte[]>> ChunkFile(FileReference File)
		{
			List<Tuple<byte[], byte[]>> Chunks = new List<Tuple<byte[], byte[]>>();
			const int ChunkSize = 10 * 1024 * 1024; // chunk size in bytes
			const int BlockSize = 4 * 1024; // the size of each block we read
			byte[] FileBlock = new byte[BlockSize];

			using (FileStream FileStream = new FileStream(File.FullName, FileMode.Open))
			{
				bool MoreDataAvailable = true;
				while (MoreDataAvailable)
				{
					int TotalReadCountOfBytes = 0;
					long RemainingFileSize = FileStream.Length - FileStream.Position;
					if (RemainingFileSize == 0)
					{
						MoreDataAvailable = false;
						continue;
					}

					int CurrentChunkSize = Math.Min((int)RemainingFileSize, ChunkSize);
					byte[] Chunk = new byte[CurrentChunkSize];

					while (TotalReadCountOfBytes < CurrentChunkSize)
					{
						int ReadCountOfBytes = FileStream.Read(FileBlock, 0, BlockSize);
						if (ReadCountOfBytes == 0)
						{
							MoreDataAvailable = false;
							break;
						}
						Array.Copy(FileBlock, 0, Chunk, TotalReadCountOfBytes, ReadCountOfBytes);

						TotalReadCountOfBytes += ReadCountOfBytes;
					}

					byte[] Hash = SHA1Utils.GetSHA1(Chunk);
					Chunks.Add(new Tuple<byte[], byte[]>(Hash, Chunk));
				}
			}

			return Chunks;
		}
	}

	public static class SHA1Utils
	{
		public static byte[] GetSHA1(byte[] payload)
		{
			using (SHA1 sha1 = SHA1.Create())
			{
				return sha1.ComputeHash(payload);
			}
		}

		public static string GetSHA1AsString(byte[] payload)
		{
			byte[] sha1 = GetSHA1(payload);

			return FormatAsHexString(sha1);
		}

		private static int GetHexVal(char hex)
		{
			return hex - (hex < 58 ? 48 : hex < 97 ? 55 : 87);
		}

		public static byte[] ToSha1FromString(string hex)
		{
			if (hex.Length % 2 == 1)
			{
				throw new Exception("The binary key cannot have an odd number of digits");
			}

			byte[] arr = new byte[hex.Length >> 1];

			for (int i = 0; i < hex.Length >> 1; ++i)
			{
				arr[i] = (byte)((GetHexVal(hex[i << 1]) << 4) + GetHexVal(hex[(i << 1) + 1]));
			}

			return arr;
		}

		public static string FormatAsHexString(byte[] sha1Hash)
		{
			StringBuilder sb = new StringBuilder();
			foreach (byte b in sha1Hash)
			{
				string hex = b.ToString("X2");
				sb.Append(hex);
			}

			return sb.ToString();
		}
	}
}
