// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Text;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Horde.Storage.Implementation;
using Horde.Storage.Implementation.Blob;
using Jupiter;
using Jupiter.Common;
using Jupiter.Implementation;
using Microsoft.AspNetCore.Http;
using Microsoft.Extensions.Options;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Moq;

namespace Horde.Storage.UnitTests
{
    using BlobNotFoundException = Horde.Storage.Implementation.BlobNotFoundException;
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
        
        private readonly BlobIdentifier _onlyFirstId = new BlobIdentifier(new string('1', 40));
        private readonly BlobIdentifier _onlySecondId = new BlobIdentifier(new string('2', 40));
        private readonly BlobIdentifier _onlyThirdId = new BlobIdentifier(new string('3', 40));
        private readonly BlobIdentifier _allId = new BlobIdentifier(new string('4', 40));
        private readonly BlobIdentifier _onlyFirstUniqueNsId = new BlobIdentifier(new string('5', 40));
        private readonly BlobIdentifier _onlySecondUniqueNsId = new BlobIdentifier(new string('6', 40));
        private readonly BlobIdentifier _nonExisting = new BlobIdentifier(new string('0', 40));

        public HierarchicalBlobStorageTest()
        {

        }

        [TestInitialize]
        public async Task Setup()
        {
            Mock<IServiceProvider> serviceProviderMock = new Mock<IServiceProvider>();
            using MemoryCacheBlobStore blobStore =  new MemoryCacheBlobStore(Mock.Of<IOptionsMonitor<MemoryCacheBlobSettings>>(_ => _.CurrentValue == new MemoryCacheBlobSettings()));
            serviceProviderMock.Setup(x => x.GetService(typeof(MemoryCacheBlobStore))).Returns(blobStore);
            IOptionsMonitor<HordeStorageSettings> settingsMonitor = Mock.Of<IOptionsMonitor<HordeStorageSettings>>(_ => _.CurrentValue == new HordeStorageSettings());
            Mock<INamespacePolicyResolver> mockPolicyResolver = new Mock<INamespacePolicyResolver>();
            mockPolicyResolver.Setup(x => x.GetPoliciesForNs(It.IsAny<NamespaceId>())).Returns(new NamespacePolicy());

            _chained = new BlobService(serviceProviderMock.Object, settingsMonitor, Mock.Of<IBlobIndex>(), Mock.Of<IPeerStatusService>(), Mock.Of<IHttpClientFactory>(), Mock.Of<IServiceCredentials>(), mockPolicyResolver.Object, Mock.Of<IHttpContextAccessor>());
            _chained.BlobStore = new List<IBlobStore> { _first, _second, _third };

            await _first.PutObject(Ns, Encoding.ASCII.GetBytes("onlyFirstContent"), _onlyFirstId);
            await _second.PutObject(Ns, Encoding.ASCII.GetBytes("onlySecondContent"), _onlySecondId);
            await _third.PutObject(Ns, Encoding.ASCII.GetBytes("onlyThirdContent"), _onlyThirdId);
            
            await _first.PutObject(Ns, Encoding.ASCII.GetBytes("allContent"), _allId);
            await _second.PutObject(Ns, Encoding.ASCII.GetBytes("allContent"), _allId);
            await _third.PutObject(Ns, Encoding.ASCII.GetBytes("allContent"), _allId);
            await _first.PutObject(NsOnlyFirst, Encoding.ASCII.GetBytes("onlyFirstUniqueNs"), _onlyFirstUniqueNsId);
            await _second.PutObject(NsOnlySecond, Encoding.ASCII.GetBytes("onlySecondUniqueNs"), _onlySecondUniqueNsId);
        }
        
        [TestMethod]
        public async Task PutObject()
        {
            BlobIdentifier new1 = new BlobIdentifier("A418A2821A76B110092C9745151E112253F7999B");
            Assert.IsFalse(await _chained.Exists(Ns, new1));
            await _chained.PutObject(Ns, Encoding.ASCII.GetBytes("new1"), new1);
            Assert.IsTrue(await _chained.Exists(Ns, new1));
            Assert.IsTrue(await _first.Exists(Ns, new1));
            Assert.IsTrue(await _second.Exists(Ns, new1));
        }
        
        [TestMethod]
        public async Task GetObject()
        {
            Assert.AreEqual("onlyFirstContent", BlobToString(await _chained.GetObject(Ns, _onlyFirstId)));
            Assert.AreEqual("onlySecondContent", BlobToString(await _chained.GetObject(Ns, _onlySecondId)));
            Assert.AreEqual("onlyFirstUniqueNs", BlobToString(await _chained.GetObject(NsOnlyFirst, _onlyFirstUniqueNsId)));
            Assert.AreEqual("onlySecondUniqueNs", BlobToString(await _chained.GetObject(NsOnlySecond, _onlySecondUniqueNsId)));
            Assert.AreEqual("allContent", BlobToString(await _chained.GetObject(Ns, _allId)));
            await Assert.ThrowsExceptionAsync<BlobNotFoundException>(() => _chained.GetObject(Ns, _nonExisting));
            await Assert.ThrowsExceptionAsync<NamespaceNotFoundException>(() => _chained.GetObject(new NamespaceId("non-existing-ns"), _nonExisting));

            // verify that the objects have propagated
            Assert.AreEqual(3, _first.GetIdentifiers(Ns).Count());
            Assert.AreEqual(2, _second.GetIdentifiers(Ns).Count());
            Assert.AreEqual(2, _third.GetIdentifiers(Ns).Count());
        }
        
        [TestMethod]
        public async Task PopulateHierarchy()
        {
            Assert.IsFalse(await _first.Exists(Ns, _onlyThirdId));
            Assert.IsFalse(await _second.Exists(Ns, _onlyThirdId));
            Assert.IsTrue(await _third.Exists(Ns, _onlyThirdId));
            
            // Should populate 'first' and 'second' as they are higher up in the hierarchy
            Assert.AreEqual("onlyThirdContent", BlobToString(await _chained.GetObject(Ns, _onlyThirdId)));
            
            Assert.AreEqual("onlyThirdContent", BlobToString(await _first.GetObject(Ns, _onlyThirdId)));
            Assert.AreEqual("onlyThirdContent", BlobToString(await _second.GetObject(Ns, _onlyThirdId)));
            Assert.AreEqual("onlyThirdContent", BlobToString(await _third.GetObject(Ns, _onlyThirdId)));
        }
        
        [TestMethod]
        public async Task Exists()
        {
            Assert.IsTrue(await _chained.Exists(Ns, _onlyFirstId));
            Assert.IsTrue(await _chained.Exists(Ns, _onlySecondId));
            Assert.IsTrue(await _chained.Exists(Ns, _allId));
            Assert.IsFalse(await _chained.Exists(Ns, _nonExisting));
        }
        
        [TestMethod]
        public async Task DeleteObject()
        {
            await Assert.ThrowsExceptionAsync<NamespaceNotFoundException>(() => _chained.DeleteObject(NsnonExistingNs, _nonExisting));
            await Assert.ThrowsExceptionAsync<BlobNotFoundException>(() => _chained.DeleteObject(NsOnlyFirst, _nonExisting));
            await Assert.ThrowsExceptionAsync<BlobNotFoundException>(() => _chained.DeleteObject(NsOnlySecond, _nonExisting));
            
            Assert.IsTrue(await _first.Exists(Ns, _onlyFirstId));
            await _chained.DeleteObject(Ns, _onlyFirstId);
            Assert.IsFalse(await _first.Exists(Ns, _onlyFirstId));
            
            Assert.IsTrue(await _second.Exists(NsOnlySecond, _onlySecondUniqueNsId));
            await _chained.DeleteObject(NsOnlySecond, _onlySecondUniqueNsId);
            Assert.IsFalse(await _second.Exists(NsOnlySecond, _onlySecondUniqueNsId));
            
            Assert.IsTrue(await _first.Exists(Ns, _allId));
            Assert.IsTrue(await _second.Exists(Ns, _allId));
            await _chained.DeleteObject(Ns, _allId);
            Assert.IsFalse(await _first.Exists(Ns, _allId));
            Assert.IsFalse(await _second.Exists(Ns, _allId));
        }
        
        [TestMethod]
        public async Task DeleteNamespace()
        {
            await Assert.ThrowsExceptionAsync<NamespaceNotFoundException>(() => _chained.DeleteNamespace(NsnonExistingNs));

            Assert.IsTrue(await _first.Exists(Ns, _allId));
            Assert.IsTrue(await _second.Exists(Ns, _allId));
            await _chained.DeleteNamespace(Ns);
            await Assert.ThrowsExceptionAsync<NamespaceNotFoundException>(() => _first.DeleteNamespace(Ns));
            await Assert.ThrowsExceptionAsync<NamespaceNotFoundException>(() => _second.DeleteNamespace(Ns));
        }

        private static string BlobToString(BlobContents contents)
        {
            using StreamReader reader = new StreamReader(contents.Stream);
            return reader.ReadToEnd();
        }
    }
}
