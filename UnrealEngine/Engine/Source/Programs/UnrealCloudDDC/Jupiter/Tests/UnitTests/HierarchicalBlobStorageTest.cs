// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Text;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Jupiter.Implementation;
using Jupiter.Implementation.Blob;
using Jupiter.Common;
using Jupiter.Common.Implementation;
using Microsoft.AspNetCore.Http;
using Microsoft.Extensions.Options;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Moq;
using OpenTelemetry.Trace;
using Microsoft.Extensions.Logging.Abstractions;

namespace Jupiter.UnitTests
{
	using BlobNotFoundException = Jupiter.Implementation.BlobNotFoundException;
	using IBlobStore = Implementation.IBlobStore;

	[TestClass]
	public class HierarchicalBlobStorageTest
	{
		private readonly NamespaceId Ns = new NamespaceId("my-namespace");
		private readonly NamespaceId NsnonExistingNs = new NamespaceId("non-existing-ns");
		private readonly NamespaceId NsOnlyFirst = new NamespaceId("ns-only-in-first");
		private readonly NamespaceId NsOnlySecond = new NamespaceId("ns-only-in-second");
		private readonly MemoryBlobStore _first = new MemoryBlobStore(throwOnOverwrite: false);
		private readonly MemoryBlobStore _second = new MemoryBlobStore(throwOnOverwrite: false);
		private readonly MemoryBlobStore _third = new MemoryBlobStore(throwOnOverwrite: false);
		private BlobService _chained = null!;
		
		private readonly BlobId _onlyFirstId = new BlobId(new string('1', 40));
		private readonly BlobId _onlySecondId = new BlobId(new string('2', 40));
		private readonly BlobId _onlyThirdId = new BlobId(new string('3', 40));
		private readonly BlobId _allId = new BlobId(new string('4', 40));
		private readonly BlobId _onlyFirstUniqueNsId = new BlobId(new string('5', 40));
		private readonly BlobId _onlySecondUniqueNsId = new BlobId(new string('6', 40));
		private readonly BlobId _nonExisting = new BlobId(new string('0', 40));

		public HierarchicalBlobStorageTest()
		{

		}

		[TestInitialize]
		public async Task SetupAsync()
		{
			Mock<IServiceProvider> serviceProviderMock = new Mock<IServiceProvider>();
			MemoryBlobStore blobStore =  new MemoryBlobStore();
			serviceProviderMock.Setup(x => x.GetService(typeof(MemoryBlobStore))).Returns(blobStore);
			IOptionsMonitor<UnrealCloudDDCSettings> settingsMonitor = Mock.Of<IOptionsMonitor<UnrealCloudDDCSettings>>(_ => _.CurrentValue == new UnrealCloudDDCSettings());
			IOptionsMonitor<JupiterSettings> jupiterSettingsMonitor = Mock.Of<IOptionsMonitor<JupiterSettings>>(_ => _.CurrentValue == new JupiterSettings());
			Mock<INamespacePolicyResolver> mockPolicyResolver = new Mock<INamespacePolicyResolver>();
			mockPolicyResolver.Setup(x => x.GetPoliciesForNs(It.IsAny<NamespaceId>())).Returns(new NamespacePolicy());

			Tracer tracer = TracerProvider.Default.GetTracer("TestTracer");
			IOptionsMonitor<BufferedPayloadOptions> bufferedPayloadOptions = Mock.Of<IOptionsMonitor<BufferedPayloadOptions>>(_ => _.CurrentValue == new BufferedPayloadOptions());
			BufferedPayloadFactory bufferedPayloadFactory = new BufferedPayloadFactory(bufferedPayloadOptions, tracer);

			_chained = new BlobService(serviceProviderMock.Object, settingsMonitor, jupiterSettingsMonitor, Mock.Of<IBlobIndex>(), Mock.Of<IPeerStatusService>(), Mock.Of<IHttpClientFactory>(), Mock.Of<IServiceCredentials>(), mockPolicyResolver.Object, Mock.Of<IHttpContextAccessor>(), null, tracer, bufferedPayloadFactory, NullLogger<BlobService>.Instance, null);
			_chained.BlobStore = new List<IBlobStore> { _first, _second, _third };

			await _first.PutObjectAsync(Ns, Encoding.ASCII.GetBytes("onlyFirstContent"), _onlyFirstId);
			await _second.PutObjectAsync(Ns, Encoding.ASCII.GetBytes("onlySecondContent"), _onlySecondId);
			await _third.PutObjectAsync(Ns, Encoding.ASCII.GetBytes("onlyThirdContent"), _onlyThirdId);
			
			await _first.PutObjectAsync(Ns, Encoding.ASCII.GetBytes("allContent"), _allId);
			await _second.PutObjectAsync(Ns, Encoding.ASCII.GetBytes("allContent"), _allId);
			await _third.PutObjectAsync(Ns, Encoding.ASCII.GetBytes("allContent"), _allId);
			await _first.PutObjectAsync(NsOnlyFirst, Encoding.ASCII.GetBytes("onlyFirstUniqueNs"), _onlyFirstUniqueNsId);
			await _second.PutObjectAsync(NsOnlySecond, Encoding.ASCII.GetBytes("onlySecondUniqueNs"), _onlySecondUniqueNsId);
		}
		
		[TestMethod]
		public async Task PutObjectAsync()
		{
			BlobId new1 = new BlobId("A418A2821A76B110092C9745151E112253F7999B");
			Assert.IsFalse(await _chained.ExistsAsync(Ns, new1));
			await _chained.PutObjectAsync(Ns, Encoding.ASCII.GetBytes("new1"), new1);
			Assert.IsTrue(await _chained.ExistsAsync(Ns, new1));
			Assert.IsTrue(await _first.ExistsAsync(Ns, new1));
			Assert.IsTrue(await _second.ExistsAsync(Ns, new1));
		}
		
		[TestMethod]
		public async Task GetObjectAsync()
		{
			Assert.AreEqual("onlyFirstContent", BlobToString(await _chained.GetObjectAsync(Ns, _onlyFirstId)));
			Assert.AreEqual("onlySecondContent", BlobToString(await _chained.GetObjectAsync(Ns, _onlySecondId)));
			Assert.AreEqual("onlyFirstUniqueNs", BlobToString(await _chained.GetObjectAsync(NsOnlyFirst, _onlyFirstUniqueNsId)));
			Assert.AreEqual("onlySecondUniqueNs", BlobToString(await _chained.GetObjectAsync(NsOnlySecond, _onlySecondUniqueNsId)));
			Assert.AreEqual("allContent", BlobToString(await _chained.GetObjectAsync(Ns, _allId)));
			await Assert.ThrowsExceptionAsync<BlobNotFoundException>(() => _chained.GetObjectAsync(Ns, _nonExisting));
			await Assert.ThrowsExceptionAsync<BlobNotFoundException>(() => _chained.GetObjectAsync(new NamespaceId("non-existing-ns"), _nonExisting));

			// verify that the objects have propagated
			Assert.AreEqual(3, _first.GetIdentifiers(Ns).Count());
			Assert.AreEqual(2, _second.GetIdentifiers(Ns).Count());
			Assert.AreEqual(2, _third.GetIdentifiers(Ns).Count());
		}
		
		[TestMethod]
		public async Task PopulateHierarchyAsync()
		{
			Assert.IsFalse(await _first.ExistsAsync(Ns, _onlyThirdId));
			Assert.IsFalse(await _second.ExistsAsync(Ns, _onlyThirdId));
			Assert.IsTrue(await _third.ExistsAsync(Ns, _onlyThirdId));
			
			// Should populate 'first' and 'second' as they are higher up in the hierarchy
			Assert.AreEqual("onlyThirdContent", BlobToString(await _chained.GetObjectAsync(Ns, _onlyThirdId)));
			
			Assert.AreEqual("onlyThirdContent", BlobToString(await _first.GetObjectAsync(Ns, _onlyThirdId)));
			Assert.AreEqual("onlyThirdContent", BlobToString(await _second.GetObjectAsync(Ns, _onlyThirdId)));
			Assert.AreEqual("onlyThirdContent", BlobToString(await _third.GetObjectAsync(Ns, _onlyThirdId)));
		}
		
		[TestMethod]
		public async Task ExistsAsync()
		{
			Assert.IsTrue(await _chained.ExistsAsync(Ns, _onlyFirstId));
			Assert.IsTrue(await _chained.ExistsAsync(Ns, _onlySecondId));
			Assert.IsTrue(await _chained.ExistsAsync(Ns, _allId));
			Assert.IsFalse(await _chained.ExistsAsync(Ns, _nonExisting));
		}
		
		[TestMethod]
		public async Task DeleteObjectAsync()
		{
			Assert.IsFalse(await _chained.ExistsAsync(NsnonExistingNs, _nonExisting));
			Assert.IsFalse(await _chained.ExistsAsync(NsOnlyFirst, _nonExisting));
			Assert.IsFalse(await _chained.ExistsAsync(NsOnlySecond, _nonExisting));
			
			Assert.IsTrue(await _first.ExistsAsync(Ns, _onlyFirstId));
			await _chained.DeleteObjectAsync(Ns, _onlyFirstId);
			Assert.IsFalse(await _first.ExistsAsync(Ns, _onlyFirstId));
			
			Assert.IsTrue(await _second.ExistsAsync(NsOnlySecond, _onlySecondUniqueNsId));
			await _chained.DeleteObjectAsync(NsOnlySecond, _onlySecondUniqueNsId);
			Assert.IsFalse(await _second.ExistsAsync(NsOnlySecond, _onlySecondUniqueNsId));
			
			Assert.IsTrue(await _first.ExistsAsync(Ns, _allId));
			Assert.IsTrue(await _second.ExistsAsync(Ns, _allId));
			await _chained.DeleteObjectAsync(Ns, _allId);
			Assert.IsFalse(await _first.ExistsAsync(Ns, _allId));
			Assert.IsFalse(await _second.ExistsAsync(Ns, _allId));
		}
		
		[TestMethod]
		public async Task DeleteNamespaceAsync()
		{
			await Assert.ThrowsExceptionAsync<NamespaceNotFoundException>(() => _chained.DeleteNamespaceAsync(NsnonExistingNs));

			Assert.IsTrue(await _first.ExistsAsync(Ns, _allId));
			Assert.IsTrue(await _second.ExistsAsync(Ns, _allId));
			await _chained.DeleteNamespaceAsync(Ns);
			await Assert.ThrowsExceptionAsync<NamespaceNotFoundException>(() => _first.DeleteNamespaceAsync(Ns));
			await Assert.ThrowsExceptionAsync<NamespaceNotFoundException>(() => _second.DeleteNamespaceAsync(Ns));
		}

		private static string BlobToString(BlobContents contents)
		{
			using StreamReader reader = new StreamReader(contents.Stream);
			return reader.ReadToEnd();
		}
	}
}
