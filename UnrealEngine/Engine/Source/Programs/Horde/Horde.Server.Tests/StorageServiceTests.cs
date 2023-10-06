// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Serialization;
using Horde.Server.Configuration;
using Horde.Server.Storage;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Server.Tests
{
	[TestClass]
	public class StorageServiceTests : TestSetup
	{
		[TestMethod]
		public async Task BlobCollectionTest()
		{
			StorageService storageService = ServiceProvider.GetRequiredService<StorageService>();
			IStorageClientImpl client = await storageService.GetClientAsync(new NamespaceId("memory"), default);

			List<BlobType> types = new List<BlobType>();
			types.Add(new BlobType(Guid.Parse("{11C2D886-3349-4164-946F-E9D10BD12E3D}"), 0));
			types.Add(new BlobType(Guid.Parse("{6CB3A005-26BA-4787-86D2-793ED13771CB}"), 0));

			byte[] data1 = new byte[] { 1, 2, 3 };
			IoHash hash1 = IoHash.Compute(data1);

			byte[] data2 = new byte[] { 4, 5, 6 };
			IoHash hash2 = IoHash.Compute(data2);

			List<BundleExport> exports = new List<BundleExport>();

			exports.Add(new BundleExport(0, hash1, 0, 0, data1.Length, Array.Empty<BundleExportRef>()));
			exports.Add(new BundleExport(0, hash1, 0, 0, data1.Length, Array.Empty<BundleExportRef>()));
			exports.Add(new BundleExport(0, hash2, 0, data1.Length, data2.Length, Array.Empty<BundleExportRef>()));

			BundleHeader header = BundleHeader.Create(types, Array.Empty<BlobLocator>(), exports, new BundlePacket[1]);
			Bundle bundle = new Bundle(header, Array.Empty<ReadOnlyMemory<byte>>());
			BlobLocator locator = await client.WriteBundleAsync(bundle);

			await client.AddAliasAsync("foo", new NodeLocator(hash1, locator, 0));
			await client.AddAliasAsync("foo", new NodeLocator(hash1, locator, 1));
			await client.AddAliasAsync("bar", new NodeLocator(hash2, locator, 2));

			List<BlobHandle> handles;
			
			handles = await client.FindNodesAsync("foo").ToListAsync();
			Assert.AreEqual(2, handles.Count);
			Assert.AreEqual(new NodeLocator(hash1, locator, 0), handles[0].GetLocator());
			Assert.AreEqual(new NodeLocator(hash1, locator, 1), handles[1].GetLocator());

			handles = await client.FindNodesAsync("bar").ToListAsync();
			Assert.AreEqual(1, handles.Count);
			Assert.AreEqual(new NodeLocator(hash2, locator, 2), handles[0].GetLocator());
		}
	}
}

