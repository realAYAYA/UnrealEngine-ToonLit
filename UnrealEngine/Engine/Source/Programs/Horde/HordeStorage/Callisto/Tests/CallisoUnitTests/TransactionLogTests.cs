// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.IO;
using System.Linq;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System.Threading.Tasks;
using Callisto.Implementation;
using EpicGames.Horde.Storage;
using Jupiter.Implementation;

namespace Calliso.UnitTests
{
    [TestClass]
    public class TransactionLogTests
    {
        private readonly DirectoryInfo _tempPath = new DirectoryInfo(Path.GetTempPath());
        private readonly NamespaceId _namespace = new NamespaceId("test-namespace");

        [TestInitialize]
        public void CleanupTempDir()
        {
            string tempDir = Path.Combine(_tempPath.FullName, _namespace.ToString());
            if (Directory.Exists(tempDir))
            {
                Directory.Delete(tempDir, true);
            }
        }

        [TestMethod]
        public async Task MemoryTransactionLog()
        {
            MemoryTransactionLog log = new MemoryTransactionLog();
            AddTransactionEvent @event = new AddTransactionEvent("Foo", "Bar",
                blobs: new[] {new BlobIdentifier("0BEEC7B5EA3F0FDBC95D0DD47F3C5BC275DA8A33"), },
                locations: new[] {"ARN"}.ToList());

            long insertedIndex = await log.NewTransaction(@event);
            Assert.AreEqual(0, insertedIndex);

            TransactionEvent event2 = (await log.Get(0, 1, null)).Events.First();
            Assert.AreEqual(@event.Name, event2.Name);

            Assert.IsInstanceOfType(event2, expectedType: typeof(TransactionEvent));
            AddTransactionEvent addEvent = (AddTransactionEvent) event2;
            CollectionAssert.AreEqual(@event.Blobs, addEvent.Blobs);
        }

        [TestMethod]
        public async Task FileTransactionLog()
        {
            CleanupTempDir();

            FileTransactionLog log = new FileTransactionLog(_tempPath, _namespace);
            AddTransactionEvent @event = new AddTransactionEvent("Foo", "Bar",
                blobs: new[] {new BlobIdentifier("0BEEC7B5EA3F0FDBC95D0DD47F3C5BC275DA8A33") },
                locations: new[] {"ARN"}.ToList());

            long insertedIndex = await log.NewTransaction(@event);
            Assert.AreEqual(0, insertedIndex);

            TransactionEvent[] events = (await log.Get(0, 1)).Events;
            TransactionEvent event2 = events[0];

            Assert.AreEqual(@event.Name, event2.Name);
            Assert.IsInstanceOfType(event2, expectedType: typeof(TransactionEvent));
            AddTransactionEvent addEvent = (AddTransactionEvent) event2;
            CollectionAssert.AreEqual(@event.Blobs, addEvent.Blobs);
            Assert.IsTrue(addEvent.Locations.Contains("ARN"));
        }

        [TestMethod]
        public async Task FileTransactionLogUnicodeEvent()
        {
            CleanupTempDir();

            FileTransactionLog log = new FileTransactionLog(_tempPath, _namespace);
            AddTransactionEvent @event = new AddTransactionEvent("😀", "😉",
                blobs: new[] {new BlobIdentifier("0BEEC7B5EA3F0FDBC95D0DD47F3C5BC275DA8A33")},
                locations: new[] {"ARN"}.ToList());
            long insertedIndex = await log.NewTransaction(@event);
            Assert.AreEqual(0, insertedIndex);

            TransactionEvent[] events = (await log.Get(0, 1)).Events;
            TransactionEvent event2 = events[0];

            Assert.AreEqual(@event.Name, event2.Name);
            Assert.IsInstanceOfType(event2, expectedType: typeof(TransactionEvent));
            AddTransactionEvent addEvent = (AddTransactionEvent) event2;
            CollectionAssert.AreEqual(@event.Blobs, addEvent.Blobs);
            Assert.IsTrue(addEvent.Locations.Contains("ARN"));
        }

        [TestMethod]
        public async Task FileTransactionLogMetadataEvent()
        {
            CleanupTempDir();

            FileTransactionLog log = new FileTransactionLog(_tempPath, _namespace);
            Dictionary<string, object> metadata = new Dictionary<string, object>
            {
                {"foo", "baz"},
                {"bar", "baz"}
            };
            AddTransactionEvent @event = new AddTransactionEvent("Foo", "Bar",
                blobs: new[] { new BlobIdentifier("0BEEC7B5EA3F0FDBC95D0DD47F3C5BC275DA8A33") },
                locations: new[] {"ARN"}.ToList(), metadata);

            long insertedIndex = await log.NewTransaction(@event);
            Assert.AreEqual(0, insertedIndex);

            TransactionEvent[] events = (await log.Get(0, 1)).Events;
            TransactionEvent event2 = events[0];

            Assert.AreEqual(@event.Name, event2.Name);
            Assert.IsInstanceOfType(event2, expectedType: typeof(TransactionEvent));
            AddTransactionEvent addEvent = (AddTransactionEvent) event2;
            CollectionAssert.AreEqual(@event.Blobs, addEvent.Blobs);
            Assert.IsTrue(addEvent.Locations.Contains("ARN"));

            Assert.IsNotNull(addEvent.Metadata);
            CollectionAssert.AreEqual(metadata, addEvent.Metadata);
        }

        [TestMethod]
        public async Task FileTransactionLogMultipleEvents()
        {
            CleanupTempDir();

            const long Index1 = 0;
            const long Index2 = 71;
            const long Index3 = 162;

            FileTransactionLog log = new FileTransactionLog(_tempPath, _namespace);
            AddTransactionEvent event1 = new AddTransactionEvent("Foo1", "Bar",
                blobs: new[] {new BlobIdentifier("0BEEC7B5EA3F0FDBC95D0DD47F3C5BC275DA8A33")},
                locations: new[] {"ARN"}.ToList(), identifier: Index1);
            AddTransactionEvent event2 = new AddTransactionEvent( "Foo2", "Bar",
                blobs: new[] {new BlobIdentifier("62CDB7020FF920E5AA642C3D4066950DD1F01F4D"), new BlobIdentifier("7855AA267FABC5CCD3891282DC2EAE8BAEEEC087") },
                locations: new[] {"ARN"}.ToList(), identifier: Index2);
            RemoveTransactionEvent event3 =
                new RemoveTransactionEvent("Foo3", "Bar", locations: new[] {"ARN"}.ToList(), Index3);

            TransactionEvent[] originalEvents = {event1, event2, event3};

            long insertedIndex1 = await log.NewTransaction(event1);
            long insertedIndex2 = await log.NewTransaction(event2);
            long insertedIndex3 = await log.NewTransaction(event3);

            const int expectedOffsetIncrement = 71;
            Assert.AreEqual(Index1, insertedIndex1);
            Assert.AreEqual(Index2, insertedIndex2);
            Assert.AreEqual(Index3, insertedIndex3);

            // we get the event in the middle
            TransactionEvent[] transactionEvents = (await log.Get(expectedOffsetIncrement, 1)).Events;
            AddTransactionEvent addTransactionEvent = (AddTransactionEvent) transactionEvents[0];

            Assert.AreEqual(event2.Name, addTransactionEvent.Name);
            CollectionAssert.AreEqual(event2.Blobs, addTransactionEvent.Blobs);

            transactionEvents = (await log.Get(0, 3)).Events;

            for (int i = 0; i < 3; i++)
            {
                TransactionEvent transactionEvent = transactionEvents[i];
                TransactionEvent originalEvent = originalEvents[i];

                Assert.AreEqual(originalEvent.Name, transactionEvent.Name);
                Assert.AreEqual(originalEvent.Bucket, transactionEvent.Bucket);
            }
        }

        [TestMethod]
        public async Task FileTransactionLogSiteFilter()
        {
            CleanupTempDir();

            const int Index1 = 0;
            const int Index2 = 75;
            const int Index3 = 149;

            FileTransactionLog log = new FileTransactionLog(_tempPath, _namespace);
            AddTransactionEvent arnEvent = new AddTransactionEvent("ARNEvent", "Bar",
                blobs: new[] {new BlobIdentifier("0BEEC7B5EA3F0FDBC95D0DD47F3C5BC275DA8A33") },
                locations: new[] {"ARN"}.ToList(), identifier: Index1);
            AddTransactionEvent hqAdd = new AddTransactionEvent("HqEvent", "Bar",
                blobs: new[] {new BlobIdentifier("62CDB7020FF920E5AA642C3D4066950DD1F01F4D")},
                locations: new[] {"HQ"}.ToList(), identifier: Index2);
            RemoveTransactionEvent hqRemove =
                new RemoveTransactionEvent("Foo", "Bar", locations: new[] {"HQ"}.ToList(), identifier: Index3);

            long insertedIndex1 = await log.NewTransaction(arnEvent);
            long insertedIndex2 = await log.NewTransaction(hqAdd);
            long insertedIndex3 = await log.NewTransaction(hqRemove);

            Assert.AreEqual(Index1, insertedIndex1);
            Assert.AreEqual(Index2, insertedIndex2);
            Assert.AreEqual(Index3, insertedIndex3);

            // we get the event in the middle
            TransactionEvent[] transactionEvents = (await log.Get(0, 1, "ARN")).Events;
            Assert.AreEqual(1, transactionEvents.Length);
            Assert.IsNotNull(transactionEvents[0]);
            AddTransactionEvent addTransactionEvent = (AddTransactionEvent) transactionEvents[0];

            // this should have matched the second event even though we set a start index matching the beginning
            Assert.AreEqual(hqAdd.Name, addTransactionEvent.Name);
            CollectionAssert.AreEqual(hqAdd.Blobs, addTransactionEvent.Blobs);

            transactionEvents = (await log.Get(0, 3, "HQ")).Events;

            // this should only return 1 event because only one event has not been at HQ
            Assert.AreEqual(1, transactionEvents.Length);
            TransactionEvent transactionEvent = transactionEvents[0];

            Assert.AreEqual(arnEvent.Name, transactionEvent.Name);
            Assert.AreEqual(arnEvent.Bucket, transactionEvent.Bucket);
        }

        [TestMethod]
        public async Task FileTransactionLogMultipleSites()
        {
            CleanupTempDir();

            FileTransactionLog log = new FileTransactionLog(_tempPath, _namespace);
            AddTransactionEvent arnEvent = new AddTransactionEvent("ARNEvent", "Bar",
                blobs: new[] { new BlobIdentifier("0BEEC7B5EA3F0FDBC95D0DD47F3C5BC275DA8A33") },
                locations: new[] { "ARN", "HQ", "USE" }.ToList());

            long insertedIndex = await log.NewTransaction(arnEvent);

            Assert.AreEqual(0, insertedIndex);

            TransactionEvent[] transactionEvents = (await log.Get(0, 1)).Events;
            Assert.AreEqual(1, transactionEvents.Length);
            Assert.IsNotNull(transactionEvents[0]);
            AddTransactionEvent addTransactionEvent = (AddTransactionEvent)transactionEvents[0];

            Assert.AreEqual(arnEvent.Name, addTransactionEvent.Name);
            Assert.AreEqual(arnEvent.Bucket, addTransactionEvent.Bucket);
            CollectionAssert.AreEqual(new[] {"ARN", "HQ", "USE"}, addTransactionEvent.Locations);

            Assert.AreEqual(0, log.IndexFile.ToIndex("ARN"));
            Assert.AreEqual(1, log.IndexFile.ToIndex("HQ"));
            Assert.AreEqual(2, log.IndexFile.ToIndex("USE"));

            FileTransactionLog.OpRecord opRecord = Callisto.Implementation.FileTransactionLog.AddOp.FromEvent(log.IndexFile, addTransactionEvent);
            FileTransactionLog.AddOp addOp = (FileTransactionLog.AddOp) opRecord.Op!;
            Assert.AreEqual(7UL, addOp.Locations);
        }

        [TestMethod]
        public async Task FileTransactionLogPartialSites()
        {
            CleanupTempDir();

            FileTransactionLog log = new FileTransactionLog(_tempPath, _namespace);

            // set the log file indexes
            Assert.AreEqual(0, log.IndexFile.ToIndex("ARN"));
            Assert.AreEqual(1, log.IndexFile.ToIndex("HQ"));
            Assert.AreEqual(2, log.IndexFile.ToIndex("USE"));

            // define an event that has not been seen by all indexes
            AddTransactionEvent arnEvent = new AddTransactionEvent("ARNEvent", "Bar",
                blobs: new[] { new BlobIdentifier("0BEEC7B5EA3F0FDBC95D0DD47F3C5BC275DA8A33") },
                locations: new[] { "ARN", "USE" }.ToList());

            long insertedIndex = await log.NewTransaction(arnEvent);

            Assert.AreEqual(0, insertedIndex);

            TransactionEvent[] transactionEvents = (await log.Get(0, 1)).Events;
            Assert.AreEqual(1, transactionEvents.Length);
            Assert.IsNotNull(transactionEvents[0]);
            AddTransactionEvent addTransactionEvent = (AddTransactionEvent)transactionEvents[0];

            Assert.AreEqual(arnEvent.Name, addTransactionEvent.Name);
            Assert.AreEqual(arnEvent.Bucket, addTransactionEvent.Bucket);
            CollectionAssert.AreEqual(new[] { "ARN", "USE" }, addTransactionEvent.Locations);

            Assert.AreEqual(0, log.IndexFile.ToIndex("ARN"));
            Assert.AreEqual(2, log.IndexFile.ToIndex("USE"));

            FileTransactionLog.OpRecord opRecord = Callisto.Implementation.FileTransactionLog.AddOp.FromEvent(log.IndexFile, addTransactionEvent);
            FileTransactionLog.AddOp addOp = (FileTransactionLog.AddOp)opRecord.Op!;
            Assert.AreEqual(5UL, addOp.Locations);
        }
    }
}
