// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Text;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Horde.Storage.Implementation;
using Jupiter.Implementation;
using Microsoft.Extensions.Options;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Storage.UnitTests
{
    public class TestOptionsMonitor<T> : IOptionsMonitor<T>
        where T : class, new()
    {
        public TestOptionsMonitor(T currentValue)
        {
            CurrentValue = currentValue;
        }

        public T Get(string name)
        {
            return CurrentValue;
        }

        public IDisposable OnChange(Action<T, string> listener)
        {
            throw new NotImplementedException();
        }

        public T CurrentValue { get; }
    }
    
    [TestClass]
    public class MemoryCacheBlobStoreTest: IDisposable
    {
        private readonly NamespaceId Ns1 = new NamespaceId("my-namespace-1");
        private readonly NamespaceId Ns2 = new NamespaceId("my-namespace-2");
        private readonly BlobIdentifier _id1 = new BlobIdentifier(new string('1', 40));
        private readonly BlobIdentifier _id2 = new BlobIdentifier(new string('2', 40));
        private readonly BlobIdentifier _id3 = new BlobIdentifier(new string('3', 40));

        private readonly MemoryCacheBlobStore _store;

        public MemoryCacheBlobStoreTest()
        {
            MemoryCacheBlobSettings settings = new MemoryCacheBlobSettings();
            _store = new MemoryCacheBlobStore(new TestOptionsMonitor<MemoryCacheBlobSettings>(settings));
            _store.PutObject(Ns1, Encoding.ASCII.GetBytes("first content"), _id1);
            _store.PutObject(Ns1, Encoding.ASCII.GetBytes("second content"), _id2);
            _store.PutObject(Ns2, Encoding.ASCII.GetBytes("third content"), _id3);
        }
        
        [TestMethod]
        public async Task PutObject()
        {
            BlobIdentifier new1 = new BlobIdentifier(new string('x', 40));
            byte[] data = Encoding.ASCII.GetBytes("some new content");
            await _store.PutObject(Ns1, data, new1);
            BlobContents contents = await _store.GetObject(Ns1, new1);
            await using MemoryStream tempStream = new MemoryStream();
            await contents.Stream.CopyToAsync(tempStream);
            
            Assert.AreEqual("some new content", Encoding.ASCII.GetString(tempStream.ToArray()));
        }

        [TestMethod]
        public async Task GetObject()
        {
            BlobContents contents = await _store.GetObject(Ns1, _id1);
            await using MemoryStream tempStream = new MemoryStream();
            await contents.Stream.CopyToAsync(tempStream);
            
            Assert.AreEqual("first content", Encoding.ASCII.GetString(tempStream.ToArray()));
            Assert.AreEqual(13, contents.Length);
        }
        
        [TestMethod]
        public async Task DeleteObject()
        {
            Assert.IsTrue(await _store.Exists(Ns1, _id1));
            await _store.DeleteObject(Ns1, _id1);
            Assert.IsFalse(await _store.Exists(Ns1, _id1));
        }
        
        [TestMethod]
        public async Task Exists()
        {
            Assert.IsTrue(await _store.Exists(Ns1, _id1));
            Assert.IsFalse(await _store.Exists(Ns1, new BlobIdentifier(new string('o', 40))));
        }
        
        [TestMethod]
        [Ignore("Always returns 0")]
        public void GetMemoryCacheSize()
        {
            Assert.AreEqual(2, _store.GetMemoryCacheSize());
        }
        
        // Ignored as they are not implemented
        [TestMethod]
        [Ignore("Not implemented due to limitations with MS MemoryCache")]
        public async Task DeleteNamespace()
        {
            Assert.IsTrue(await _store.Exists(Ns1, _id1));
            Assert.IsTrue(await _store.Exists(Ns2, _id3));
            await _store.DeleteNamespace(Ns2);
            Assert.IsTrue(await _store.Exists(Ns1, _id1));
            Assert.IsFalse(await _store.Exists(Ns2, _id3));
        }

        [TestMethod]
        [Ignore("Not implemented due to limitations with MS MemoryCache")]
        public void ListOldObjects()
        {
        }

        protected virtual void Dispose(bool disposing)
        {
            if (disposing)
            {
                _store.Dispose();
            }
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }
    }
}
