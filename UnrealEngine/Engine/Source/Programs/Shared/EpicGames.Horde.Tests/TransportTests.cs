// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Compute;
using EpicGames.Horde.Compute.Transports;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace EpicGames.Horde.Tests
{
	[TestClass]
	public class TransportTests
	{
		[TestMethod]
		public async Task TestStreamTransport()
		{
			byte[] input = new byte[256 * 1024];
			new Random(0).NextBytes(input);

			byte[] payload;
			{
				using MemoryStream memoryStream = new MemoryStream();
				StreamTransport streamTransport = new StreamTransport(memoryStream);
				await streamTransport.WriteAsync(input, CancellationToken.None);
				payload = memoryStream.ToArray();
			}

			byte[] output = new byte[input.Length];
			{
				using MemoryStream memoryStream = new MemoryStream(payload);
				StreamTransport streamTransport = new StreamTransport(memoryStream);
				await CopyToAsync(streamTransport, output);
			}

			for (int idx = 0; idx < input.Length; idx++)
			{
				Assert.IsTrue(input[idx] == output[idx]);
			}
			Assert.IsTrue(input.SequenceEqual(output));

		}

		[TestMethod]
		public async Task TestAesTransport()
		{
			byte[] key = new byte[AesTransport.KeyLength];
			new Random(1).NextBytes(key);

			byte[] nonce = new byte[AesTransport.NonceLength];
			new Random(2).NextBytes(nonce);

			byte[] input = RandomNumberGenerator.GetBytes(256 * 1024);
			new Random(0).NextBytes(input);

			byte[] encrypted;

			{
				using MemoryStream memoryStream = new MemoryStream();
				StreamTransport streamTransport = new StreamTransport(memoryStream);
				await using AesTransport aesTransport = new AesTransport(streamTransport, key, nonce);
				await aesTransport.WriteAsync(input, CancellationToken.None);
				encrypted = memoryStream.ToArray();
			}

			byte[] output = new byte[input.Length];
			{
				using MemoryStream memoryStream = new MemoryStream(encrypted);
				StreamTransport streamTransport = new StreamTransport(memoryStream);
				await using AesTransport aesTransport = new AesTransport(streamTransport, key, nonce);

				for (int offset = 0; offset < output.Length;)
				{
					offset += await aesTransport.ReadPartialAsync(output.AsMemory(offset), CancellationToken.None);
				}
			}

			for (int idx = 0; idx < input.Length; idx++)
			{
				Assert.IsTrue(input[idx] == output[idx]);
			}
			Assert.IsTrue(input.SequenceEqual(output));
		}

		public static async Task CopyToAsync(IComputeTransport transport, Memory<byte> buffer)
		{
			for (int offset = 0; offset < buffer.Length;)
			{
				int size = await transport.ReadPartialAsync(buffer.Slice(offset), CancellationToken.None);
				Assert.IsTrue(size > 0);
				offset += size;
			}
		}
	}
}
