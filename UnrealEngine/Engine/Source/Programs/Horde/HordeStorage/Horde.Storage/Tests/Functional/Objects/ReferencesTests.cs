// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Net.Mime;
using System.Text;
using System.Threading.Tasks;
using Cassandra;
using Dasync.Collections;
using EpicGames.Core;
using Horde.Storage.Controllers;
using Horde.Storage.Implementation;
using Jupiter;
using Jupiter.Implementation;
using Microsoft.AspNetCore.TestHost;
using Microsoft.AspNetCore.Hosting;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using Serilog;
using Logger = Serilog.Core.Logger;
using EpicGames.Horde.Storage;
using EpicGames.Serialization;
using ContentId = Jupiter.Implementation.ContentId;

namespace Horde.Storage.FunctionalTests.References
{

    [TestClass]
    public class ScyllaReferencesTests : ReferencesTests
    {
        protected override IEnumerable<KeyValuePair<string, string>> GetSettings()
        {
            return new[]
            {
                new KeyValuePair<string, string>("Horde_Storage:ReferencesDbImplementation", HordeStorageSettings.ReferencesDbImplementations.Scylla.ToString()),
                new KeyValuePair<string, string>("Horde_Storage:ContentIdStoreImplementation", HordeStorageSettings.ContentIdStoreImplementations.Scylla.ToString()),
                new KeyValuePair<string, string>("Horde_Storage:ReplicationLogWriterImplementation", HordeStorageSettings.ReplicationLogWriterImplementations.Scylla.ToString()),
            };
        }

        protected override async Task SeedDb(IServiceProvider provider)
        {
            IReferencesStore referencesStore = provider.GetService<IReferencesStore>()!;
            //verify we are using the expected store
            Assert.IsTrue(referencesStore.GetType() == typeof(ScyllaReferencesStore));

            IContentIdStore contentIdStore = provider.GetService<IContentIdStore>()!;
            //verify we are using the expected store
            if (contentIdStore is MemoryCachedContentIdStore memoryStore)
            {
                memoryStore.Clear();
                contentIdStore = memoryStore.GetUnderlyingContentIdStore();
            }
            Assert.IsTrue(contentIdStore.GetType() == typeof(ScyllaContentIdStore));

            IReplicationLog replicationLog = provider.GetService<IReplicationLog>()!;
            //verify we are using the replication log writer
            Assert.IsTrue(replicationLog.GetType() == typeof(ScyllaReplicationLog));

            await SeedTestData();
        }

        protected override async Task TeardownDb(IServiceProvider provider)
        {
            IScyllaSessionManager scyllaSessionManager = provider.GetService<IScyllaSessionManager>()!;

            ISession replicatedKeyspace = scyllaSessionManager.GetSessionForReplicatedKeyspace();
            await replicatedKeyspace.ExecuteAsync(new SimpleStatement("DROP TABLE IF EXISTS objects"));
            await replicatedKeyspace.ExecuteAsync(new SimpleStatement("DROP TABLE IF EXISTS object_last_access"));
            await replicatedKeyspace.ExecuteAsync(new SimpleStatement("DROP TABLE IF EXISTS content_id"));

            // we need to clear out the state we modified in the replication log, otherwise the replication log tests will fail
            ISession localKeyspace = scyllaSessionManager.GetSessionForLocalKeyspace();
            await Task.WhenAll(
                // remove replication log table as we expect it to be empty when starting the tests
                localKeyspace.ExecuteAsync(new SimpleStatement("DROP TABLE IF EXISTS replication_log;")),
                // remove the snapshots
                localKeyspace.ExecuteAsync(new SimpleStatement("DROP TABLE IF EXISTS replication_snapshot;")),
                // remove the namespaces
                localKeyspace.ExecuteAsync(new SimpleStatement("DROP TABLE IF EXISTS replication_namespace;"))
            );

        }
    }

    [TestClass]
    public class MongoReferencesTests : ReferencesTests
    {
        protected override IEnumerable<KeyValuePair<string, string>> GetSettings()
        {
            return new[]
            {
                new KeyValuePair<string, string>("Horde_Storage:ReferencesDbImplementation", HordeStorageSettings.ReferencesDbImplementations.Mongo.ToString()),
                new KeyValuePair<string, string>("Horde_Storage:ContentIdStoreImplementation", HordeStorageSettings.ContentIdStoreImplementations.Mongo.ToString()),
                // we do not have a mongo version of the replication log, as the mongo deployment is only intended for single servers
                new KeyValuePair<string, string>("Horde_Storage:ReplicationLogWriterImplementation", HordeStorageSettings.ReplicationLogWriterImplementations.Memory.ToString()),
            };
        }

        protected override async Task SeedDb(IServiceProvider provider)
        {
            IReferencesStore referencesStore = provider.GetService<IReferencesStore>()!;
            //verify we are using the expected store
            Assert.IsTrue(referencesStore.GetType() == typeof(MongoReferencesStore));

            IContentIdStore contentIdStore = provider.GetService<IContentIdStore>()!;
            if (contentIdStore is MemoryCachedContentIdStore memoryStore)
            {
                memoryStore.Clear();
                contentIdStore = memoryStore.GetUnderlyingContentIdStore();
            }
            //verify we are using the expected store
            Assert.IsTrue(contentIdStore.GetType() == typeof(MongoContentIdStore));

            IReplicationLog replicationLog = provider.GetService<IReplicationLog>()!;
            //verify we are using the replication log writer
            Assert.IsTrue(replicationLog.GetType() == typeof(MemoryReplicationLog));

            await SeedTestData();
        }

        protected override async Task TeardownDb(IServiceProvider provider)
        {
            await Task.CompletedTask;
            }
    }

    [TestClass]
    public class MemoryReferencesTests : ReferencesTests
    {
        protected override IEnumerable<KeyValuePair<string, string>> GetSettings()
        {
            return new[]
            {
                new KeyValuePair<string, string>("Horde_Storage:ReferencesDbImplementation", HordeStorageSettings.ReferencesDbImplementations.Memory.ToString()),
                new KeyValuePair<string, string>("Horde_Storage:ContentIdStoreImplementation", HordeStorageSettings.ContentIdStoreImplementations.Memory.ToString()),
                new KeyValuePair<string, string>("Horde_Storage:ReplicationLogWriterImplementation", HordeStorageSettings.ReplicationLogWriterImplementations.Memory.ToString()),
            };
        }

        protected override async Task SeedDb(IServiceProvider provider)
        {
            IReferencesStore referencesStore = provider.GetService<IReferencesStore>()!;
            //verify we are using the expected refs store
            Assert.IsTrue(referencesStore.GetType() == typeof(MemoryReferencesStore));

            IContentIdStore contentIdStore = provider.GetService<IContentIdStore>()!;
            if (contentIdStore is MemoryCachedContentIdStore memoryStore)
            {
                memoryStore.Clear();
                contentIdStore = memoryStore.GetUnderlyingContentIdStore();
            }
            //verify we are using the expected store
            Assert.IsTrue(contentIdStore.GetType() == typeof(MemoryContentIdStore));

            IReplicationLog replicationLog = provider.GetService<IReplicationLog>()!;
            //verify we are using the replication log writer
            Assert.IsTrue(replicationLog.GetType() == typeof(MemoryReplicationLog));

            await SeedTestData();
        }

        protected override Task TeardownDb(IServiceProvider provider)
        {
            return Task.CompletedTask;
        }
    }
    
    public abstract class ReferencesTests
    {
        public static TestServer? Server { get; private set; }
        public static HttpClient? Client { get; private set; }

        protected IBlobService Service { get; set; } = null!;
        protected IReferencesStore ReferencesStore { get; set; } = null!;
        protected NamespaceId TestNamespace { get; } = new NamespaceId("test-namespace");

        [TestInitialize]
        public async Task Setup()

        {
            IConfigurationRoot configuration = new ConfigurationBuilder()
                // we are not reading the base appSettings here as we want exact control over what runs in the tests
                .AddJsonFile("appsettings.Testing.json", true)
                .AddInMemoryCollection(GetSettings())
                .AddEnvironmentVariables()
                .Build();

            Logger logger = new LoggerConfiguration()
                .ReadFrom.Configuration(configuration)
                .CreateLogger();

            TestServer server = new TestServer(new WebHostBuilder()
                .UseConfiguration(configuration)
                .UseEnvironment("Testing")
                .UseSerilog(logger)
                .UseStartup<HordeStorageStartup>()
            );
            Client = server.CreateClient();
            Server = server;

            Service = Server.Services.GetService<IBlobService>()!;
            ReferencesStore = Server.Services.GetService<IReferencesStore>()!;
            await SeedDb(server.Services);
        }

        protected virtual async Task SeedTestData()
        {
            await Task.CompletedTask;
        }

        [TestCleanup]
        public async Task Teardown()
        {
            if (Server != null)
            {
                await TeardownDb(Server.Services);
            }
        }

        protected abstract IEnumerable<KeyValuePair<string, string>> GetSettings();

        protected abstract Task SeedDb(IServiceProvider provider);
        protected abstract Task TeardownDb(IServiceProvider provider);

        [TestMethod]
        public async Task PutGetBlob()
        {
            const string ObjectContents = "This is treated as a opaque blob";
            byte[] data = Encoding.ASCII.GetBytes(ObjectContents);
            BlobIdentifier objectHash = BlobIdentifier.FromBlob(data);
            IoHashKey key = IoHashKey.FromName("newBlobObject");
            using HttpContent requestContent = new ByteArrayContent(data);
            requestContent.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Octet);
            requestContent.Headers.Add(CommonHeaders.HashHeaderName, objectHash.ToString());

            HttpResponseMessage result = await Client!.PutAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}", UriKind.Relative), requestContent);
            result.EnsureSuccessStatusCode();

            {
                HttpResponseMessage getResponse = await Client.GetAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}.raw", UriKind.Relative));
                getResponse.EnsureSuccessStatusCode();
                await using MemoryStream ms = new MemoryStream();
                await getResponse.Content.CopyToAsync(ms);

                byte[] roundTrippedBuffer = ms.ToArray();
                string roundTrippedPayload = Encoding.ASCII.GetString(roundTrippedBuffer);

                Assert.AreEqual(ObjectContents, roundTrippedPayload);
                CollectionAssert.AreEqual(data, roundTrippedBuffer);
                Assert.AreEqual(objectHash, BlobIdentifier.FromBlob(roundTrippedBuffer));
            }

            {
                BlobIdentifier attachment;
                {
                    HttpResponseMessage getResponse = await Client.GetAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}.uecb", UriKind.Relative));
                    getResponse.EnsureSuccessStatusCode();
                    await using MemoryStream ms = new MemoryStream();
                    await getResponse.Content.CopyToAsync(ms);

                    byte[] roundTrippedBuffer = ms.ToArray();
                    CbObject cb = new CbObject(roundTrippedBuffer);
                    List<CbField> fields = cb.ToList();

                    Assert.AreEqual(2, fields.Count);
                    CbField payloadField = fields[0];
                    Assert.IsNotNull(payloadField);
                    Assert.IsTrue(payloadField.IsBinaryAttachment());
                    attachment = BlobIdentifier.FromIoHash(payloadField.AsBinaryAttachment());
                }

                {
                    HttpResponseMessage getAttachment = await Client.GetAsync(new Uri($"api/v1/blobs/{TestNamespace}/{attachment}", UriKind.Relative));
                    getAttachment.EnsureSuccessStatusCode();
                    await using MemoryStream ms = new MemoryStream();
                    await getAttachment.Content.CopyToAsync(ms);
                    byte[] roundTrippedBuffer = ms.ToArray();
                    string roundTrippedString = Encoding.ASCII.GetString(roundTrippedBuffer);

                    Assert.AreEqual(ObjectContents, roundTrippedString);
                    Assert.AreEqual(objectHash, BlobIdentifier.FromBlob(roundTrippedBuffer));
                }
            }

            {
                HttpResponseMessage getResponse = await Client.GetAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}.json", UriKind.Relative));
                getResponse.EnsureSuccessStatusCode();
                await using MemoryStream ms = new MemoryStream();
                await getResponse.Content.CopyToAsync(ms);

                byte[] roundTrippedBuffer = ms.ToArray();
                string s = Encoding.ASCII.GetString(roundTrippedBuffer);
                JObject jObject = JObject.Parse(s);
                Assert.AreEqual(2, jObject.Children().Count());
                JToken? childToken = jObject.Children().First();
                Assert.IsNotNull(childToken);
                Assert.AreEqual(JTokenType.Property, childToken.Type);
                JProperty property = jObject.Children<JProperty>().First();
                Assert.IsNotNull(property);

                Assert.AreEqual("RawHash", property!.Name);

                string value = property.Value.Value<string>()!;
                Assert.IsNotNull(value);
                Assert.AreEqual(objectHash, new BlobIdentifier(value));
            }

            {
                // request the object as a json response using accept instead of the format filter
                using HttpRequestMessage request = new(HttpMethod.Get, new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}", UriKind.Relative));
                request.Headers.Accept.Add(new MediaTypeWithQualityHeaderValue(MediaTypeNames.Application.Json));
                HttpResponseMessage getResponse = await Client.SendAsync(request);
                getResponse.EnsureSuccessStatusCode();
                Assert.AreEqual(MediaTypeNames.Application.Json, getResponse.Content.Headers.ContentType?.MediaType);

                await using MemoryStream ms = new MemoryStream();
                await getResponse.Content.CopyToAsync(ms);

                byte[] roundTrippedBuffer = ms.ToArray();
                string s = Encoding.ASCII.GetString(roundTrippedBuffer);
                JObject jObject = JObject.Parse(s);
                Assert.AreEqual(2, jObject.Children().Count());
                JToken? childToken = jObject.Children().First();
                Assert.IsNotNull(childToken);
                Assert.AreEqual(JTokenType.Property, childToken.Type);
                JProperty property = jObject.Children<JProperty>().First();
                Assert.IsNotNull(property);

                Assert.AreEqual("RawHash", property!.Name);

                string? value = property.Value.Value<string>()!;
                Assert.IsNotNull(value);
                Assert.AreEqual(objectHash, new BlobIdentifier(value));
            }

            {
                // request the object as a jupiter inlined payload
                using HttpRequestMessage request = new (HttpMethod.Get, new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}", UriKind.Relative));
                request.Headers.Accept.Add(new MediaTypeWithQualityHeaderValue(CustomMediaTypeNames.JupiterInlinedPayload));
                HttpResponseMessage getResponse = await Client.SendAsync(request);
                getResponse.EnsureSuccessStatusCode();
                Assert.AreEqual(CustomMediaTypeNames.JupiterInlinedPayload, getResponse.Content.Headers.ContentType?.MediaType);

                await using MemoryStream ms = new MemoryStream();
                await getResponse.Content.CopyToAsync(ms);
                byte[] roundTrippedBuffer = ms.ToArray();

                string roundTrippedString = Encoding.ASCII.GetString(roundTrippedBuffer);

                Assert.AreEqual(ObjectContents, roundTrippedString);
                Assert.AreEqual(objectHash, BlobIdentifier.FromBlob(roundTrippedBuffer));
            }
        }

        [TestMethod]
        public async Task PutGetCompactBinary()
        {
            CbWriter writer = new CbWriter();
            writer.BeginObject();
            writer.WriteString("stringField", "thisIsAField");
            writer.EndObject();

            byte[] objectData = writer.ToByteArray();
            BlobIdentifier objectHash = BlobIdentifier.FromBlob(objectData);
            IoHashKey key = IoHashKey.FromName("newReferenceObject");

            using HttpContent requestContent = new ByteArrayContent(objectData);
            requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);
            requestContent.Headers.Add(CommonHeaders.HashHeaderName, objectHash.ToString());

            using HttpResponseMessage result = await Client!.PutAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}.uecb", UriKind.Relative), requestContent);
            result.EnsureSuccessStatusCode();

            {
                Assert.AreEqual(CustomMediaTypeNames.UnrealCompactBinary, result!.Content.Headers.ContentType!.MediaType);
                // check that no blobs are missing
                await using MemoryStream ms = new MemoryStream();
                await result.Content.CopyToAsync(ms);
                byte[] roundTrippedBuffer = ms.ToArray();
                CbObject cb = new CbObject(roundTrippedBuffer);
                CbField needsField = cb["needs"];
                Assert.AreNotEqual(CbField.Empty, needsField);
                List<BlobIdentifier> missingBlobs = needsField.AsArray().Select(field => BlobIdentifier.FromIoHash(field.AsHash())).ToList();
                Assert.AreEqual(0, missingBlobs.Count);
            }

            {
                BucketId bucket = new BucketId("bucket");

                ObjectRecord objectRecord = await ReferencesStore.Get(TestNamespace, bucket, key, IReferencesStore.FieldFlags.IncludePayload);

                Assert.IsTrue(objectRecord.IsFinalized);
                Assert.AreEqual(key, objectRecord.Name);
                Assert.AreEqual(objectHash, objectRecord.BlobIdentifier);
                Assert.IsNotNull(objectRecord.InlinePayload);
            }

            {
                HttpResponseMessage getResponse = await Client.GetAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}.raw", UriKind.Relative));
                getResponse.EnsureSuccessStatusCode();
                await using MemoryStream ms = new MemoryStream();
                await getResponse.Content.CopyToAsync(ms);

                byte[] roundTrippedBuffer = ms.ToArray();

                CollectionAssert.AreEqual(objectData, roundTrippedBuffer);
                Assert.AreEqual(objectHash, BlobIdentifier.FromBlob(roundTrippedBuffer));
            }

            {
                HttpResponseMessage getResponse = await Client.GetAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}.uecb", UriKind.Relative));
                getResponse.EnsureSuccessStatusCode();
                await using MemoryStream ms = new MemoryStream();
                await getResponse.Content.CopyToAsync(ms);

                byte[] roundTrippedBuffer = ms.ToArray();
                CbObject cb = new CbObject(roundTrippedBuffer);
                List<CbField> fields = cb.ToList();

                Assert.AreEqual(1, fields.Count);
                CbField stringField = fields[0];
                Assert.AreEqual("stringField", stringField.Name);
                Assert.AreEqual("thisIsAField", stringField.AsString());
            }

            {
                HttpResponseMessage getResponse = await Client.GetAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}.json", UriKind.Relative));
                getResponse.EnsureSuccessStatusCode();
                await using MemoryStream ms = new MemoryStream();
                await getResponse.Content.CopyToAsync(ms);

                byte[] roundTrippedBuffer = ms.ToArray();

                string s = Encoding.ASCII.GetString(roundTrippedBuffer);
                JObject jObject = JObject.Parse(s);
                Assert.AreEqual(1, jObject.Children().Count());

                JToken? childToken = jObject.Children().First();
                Assert.IsNotNull(childToken);
                Assert.AreEqual(JTokenType.Property, childToken.Type);
                JProperty property = jObject.Children<JProperty>().First();
                Assert.IsNotNull(property);

                Assert.AreEqual("stringField", property!.Name);

                string? value = property.Value.Value<string>();
                Assert.IsNotNull(value);
                Assert.AreEqual("thisIsAField", value);
            }
        }

        
        [TestMethod]
        public async Task PutGetCompactBinaryFiltering()
        {
            CbWriter writer = new CbWriter();
            writer.BeginObject();
            writer.WriteString("stringField", "thisIsAField");
            writer.EndObject();

            byte[] objectData = writer.ToByteArray();
            BlobIdentifier objectHash = BlobIdentifier.FromBlob(objectData);
            IoHashKey key = IoHashKey.FromName("newReferenceObject");

            using HttpContent requestContent = new ByteArrayContent(objectData);
            requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);
            requestContent.Headers.Add(CommonHeaders.HashHeaderName, objectHash.ToString());

            HttpResponseMessage result = await Client!.PutAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}.uecb", UriKind.Relative), requestContent);
            result.EnsureSuccessStatusCode();

            {
                Assert.AreEqual(CustomMediaTypeNames.UnrealCompactBinary, result!.Content.Headers.ContentType!.MediaType);
                // check that no blobs are missing
                await using MemoryStream ms = new MemoryStream();
                await result.Content.CopyToAsync(ms);
                byte[] roundTrippedBuffer = ms.ToArray();
                CbObject cb = new CbObject(roundTrippedBuffer);
                CbField needsField = cb["needs"];
                Assert.AreNotEqual(CbField.Empty, needsField);
                List<BlobIdentifier> missingBlobs = needsField.AsArray().Select(field => BlobIdentifier.FromIoHash(field.AsHash())).ToList();
                Assert.AreEqual(0, missingBlobs.Count);
            }

            {
                BucketId bucket = new BucketId("bucket");

                ObjectRecord objectRecord = await ReferencesStore.Get(TestNamespace, bucket, key, IReferencesStore.FieldFlags.None);

                Assert.IsTrue(objectRecord.IsFinalized);
                Assert.AreEqual(key, objectRecord.Name);
                Assert.AreEqual(objectHash, objectRecord.BlobIdentifier);
                Assert.IsNull(objectRecord.InlinePayload);
            }
            
            {
                HttpResponseMessage getResponse = await Client.GetAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}.json?fields=name", UriKind.Relative));
                getResponse.EnsureSuccessStatusCode();
                await using MemoryStream ms = new MemoryStream();
                await getResponse.Content.CopyToAsync(ms);

                byte[] roundTrippedBuffer = ms.ToArray();

                string s = Encoding.ASCII.GetString(roundTrippedBuffer);
                JObject jObject = JObject.Parse(s);
                Assert.AreEqual(1, jObject.Children().Count());

                JToken? childToken = jObject.Children().First();
                Assert.IsNotNull(childToken);
                Assert.AreEqual(JTokenType.Property, childToken.Type);
                JProperty property = jObject.Children<JProperty>().First();
                Assert.IsNotNull(property);

                Assert.AreEqual("stringField", property!.Name);

                string? value = property.Value.Value<string>();
                Assert.IsNotNull(value);
                Assert.AreEqual("thisIsAField", value);
            }
        }

        
        [TestMethod]
        public async Task PutGetCompactBinaryHierarchy()
        {
            CbWriter childObjectWriter = new CbWriter();
            childObjectWriter.BeginObject();
            childObjectWriter.WriteString("stringField", "thisIsAField");
            childObjectWriter.EndObject();
            byte[] childObjectData = childObjectWriter.ToByteArray();
            BlobIdentifier childObjectHash = BlobIdentifier.FromBlob(childObjectData);

            CbWriter parentObjectWriter = new CbWriter();
            parentObjectWriter.BeginObject();
            parentObjectWriter.WriteObjectAttachment("childObject", childObjectHash.AsIoHash());
            parentObjectWriter.EndObject();
            byte[] parentObjectData = parentObjectWriter.ToByteArray();
            BlobIdentifier parentObjectHash = BlobIdentifier.FromBlob(parentObjectData);

            IoHashKey key = IoHashKey.FromName("newHierarchyObject");
            // this first upload should fail with the child object missing
            {
                using HttpContent requestContent = new ByteArrayContent(parentObjectData);
                requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);
                requestContent.Headers.Add(CommonHeaders.HashHeaderName, parentObjectHash.ToString());

                HttpResponseMessage result = await Client!.PutAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}.uecb", UriKind.Relative), requestContent);
                result.EnsureSuccessStatusCode();

                Assert.AreEqual(CustomMediaTypeNames.UnrealCompactBinary, result!.Content.Headers.ContentType!.MediaType);
                // check that one blobs is missing
                await using MemoryStream ms = new MemoryStream();
                await result.Content.CopyToAsync(ms);
                byte[] roundTrippedBuffer = ms.ToArray();
                CbObject cb = new CbObject(roundTrippedBuffer);
                CbField needsField = cb["needs"];
                Assert.AreNotEqual(CbField.Empty, needsField);
                List<BlobIdentifier> missingBlobs = needsField.AsArray().Select(field => BlobIdentifier.FromIoHash(field.AsHash())).ToList();
                Assert.AreEqual(1, missingBlobs.Count);
                Assert.AreEqual(childObjectHash, missingBlobs[0]);
            }

            // upload the child object
            {
                using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Put, new Uri($"api/v1/objects/{TestNamespace}/{childObjectHash}", UriKind.Relative));
                request.Headers.Accept.Add(new MediaTypeWithQualityHeaderValue(CustomMediaTypeNames.UnrealCompactBinary));

                HttpContent requestContent = new ByteArrayContent(childObjectData);
                requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);
                requestContent.Headers.Add(CommonHeaders.HashHeaderName, childObjectHash.ToString());
                request.Content = requestContent;

                HttpResponseMessage result = await Client!.SendAsync(request);
                result.EnsureSuccessStatusCode();

                Assert.AreEqual(CustomMediaTypeNames.UnrealCompactBinary, result!.Content.Headers.ContentType!.MediaType);
                // check that one blobs is missing
                await using MemoryStream ms = new MemoryStream();
                await result.Content.CopyToAsync(ms);
                byte[] roundTrippedBuffer = ms.ToArray();
                CbObject cb = new CbObject(roundTrippedBuffer);
                CbField value = cb["identifier"];
                Assert.AreNotEqual(CbField.Empty, value);
                Assert.AreEqual(childObjectHash, BlobIdentifier.FromIoHash(value.AsHash()));
            }

            // since we have now uploaded the child object putting the object again should result in no missing references
            {
                using HttpContent requestContent = new ByteArrayContent(parentObjectData);
                requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);
                requestContent.Headers.Add(CommonHeaders.HashHeaderName, parentObjectHash.ToString());

                HttpResponseMessage result = await Client!.PutAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}.uecb", UriKind.Relative), requestContent);
                result.EnsureSuccessStatusCode();

                Assert.AreEqual(result!.Content.Headers.ContentType!.MediaType, CustomMediaTypeNames.UnrealCompactBinary);
                // check that one blobs is missing
                await using MemoryStream ms = new MemoryStream();
                await result.Content.CopyToAsync(ms);
                byte[] roundTrippedBuffer = ms.ToArray();
                CbObject cb = new CbObject(roundTrippedBuffer);
                CbField needsField = cb["needs"];
                List<BlobIdentifier> missingBlobs = needsField.AsArray().Select(field => BlobIdentifier.FromIoHash(field.AsHash())).ToList();
                Assert.AreEqual(0, missingBlobs.Count);
            }

            {
                BucketId bucket = new BucketId("bucket");

                ObjectRecord objectRecord = await ReferencesStore.Get(TestNamespace, bucket, key, IReferencesStore.FieldFlags.IncludePayload);

                Assert.IsTrue(objectRecord.IsFinalized);
                Assert.AreEqual(key, objectRecord.Name);
                Assert.AreEqual(parentObjectHash, objectRecord.BlobIdentifier);
                Assert.IsNotNull(objectRecord.InlinePayload);
            }

            {
                HttpResponseMessage getResponse = await Client.GetAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}.raw", UriKind.Relative));
                getResponse.EnsureSuccessStatusCode();
                await using MemoryStream ms = new MemoryStream();
                await getResponse.Content.CopyToAsync(ms);

                byte[] roundTrippedBuffer = ms.ToArray();

                CollectionAssert.AreEqual(parentObjectData, roundTrippedBuffer);
                Assert.AreEqual(parentObjectHash, BlobIdentifier.FromBlob(roundTrippedBuffer));
            }

            {
                HttpResponseMessage getResponse = await Client.GetAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}.uecb", UriKind.Relative));
                getResponse.EnsureSuccessStatusCode();
                await using MemoryStream ms = new MemoryStream();
                await getResponse.Content.CopyToAsync(ms);

                byte[] roundTrippedBuffer = ms.ToArray();
                CbObject cb = new CbObject(roundTrippedBuffer);
                List<CbField> fields = cb.ToList();

                Assert.AreEqual(1, fields.Count);
                CbField childObjectField = fields[0];
                Assert.AreEqual("childObject", childObjectField.Name);
                Assert.AreEqual(childObjectHash, BlobIdentifier.FromIoHash(childObjectField.AsHash()));
            }

            {
                HttpResponseMessage getResponse = await Client.GetAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}.json", UriKind.Relative));
                getResponse.EnsureSuccessStatusCode();
                await using MemoryStream ms = new MemoryStream();
                await getResponse.Content.CopyToAsync(ms);

                byte[] roundTrippedBuffer = ms.ToArray();

                string s = Encoding.ASCII.GetString(roundTrippedBuffer);
                JObject jObject = JObject.Parse(s);
                Assert.AreEqual(1, jObject.Children().Count());

                JToken? childToken = jObject.Children().First();
                Assert.IsNotNull(childToken);
                Assert.AreEqual(JTokenType.Property, childToken.Type);
                JProperty property = jObject.Children<JProperty>().First();
                Assert.IsNotNull(property);

                Assert.AreEqual("childObject", property!.Name);

                string? value = property.Value.Value<string>();
                Assert.IsNotNull(value);
                Assert.AreEqual(childObjectHash.ToString().ToLower(), value);
            }

            {
                HttpResponseMessage getResponse = await Client.GetAsync(new Uri($"api/v1/objects/{TestNamespace}/{childObjectHash}", UriKind.Relative));
                getResponse.EnsureSuccessStatusCode();
                await using MemoryStream ms = new MemoryStream();
                await getResponse.Content.CopyToAsync(ms);

                byte[] roundTrippedBuffer = ms.ToArray();

                CollectionAssert.AreEqual(childObjectData, roundTrippedBuffer);
                Assert.AreEqual(childObjectHash, BlobIdentifier.FromBlob(roundTrippedBuffer));
            }
        }

        [TestMethod]
        public async Task ExistsChecks()
        {
            const string ObjectContents = "This is treated as a opaque blob";
            byte[] data = Encoding.ASCII.GetBytes(ObjectContents);
            BlobIdentifier objectHash = BlobIdentifier.FromBlob(data);
            IoHashKey key = IoHashKey.FromName("newObject");
            using HttpContent requestContent = new ByteArrayContent(data);
            requestContent.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Octet);
            requestContent.Headers.Add(CommonHeaders.HashHeaderName, objectHash.ToString());

            {
                HttpResponseMessage result = await Client!.PutAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}", UriKind.Relative), requestContent);
                result.EnsureSuccessStatusCode();
            }

            {
                using HttpRequestMessage message = new(HttpMethod.Head, new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}", UriKind.Relative));
                HttpResponseMessage result = await Client!.SendAsync(message);
                result.EnsureSuccessStatusCode();
            }

            {
                using HttpRequestMessage message = new(HttpMethod.Head, new Uri($"api/v1/refs/{TestNamespace}/bucket/{IoHashKey.FromName("missingObject")}", UriKind.Relative));
                HttpResponseMessage result = await Client!.SendAsync(message);
                Assert.AreEqual(HttpStatusCode.NotFound, result.StatusCode);
            }
        }

        [TestMethod]
        public async Task ExistsChecksMultiple()
        {
            BucketId bucket = new BucketId("bucket");
            IoHashKey existingObject = IoHashKey.FromName("existingObject");

            const string ObjectContents = "This is treated as a opaque blob";
            byte[] data = Encoding.ASCII.GetBytes(ObjectContents);
            BlobIdentifier objectHash = BlobIdentifier.FromBlob(data);
            using HttpContent requestContent = new ByteArrayContent(data);
            requestContent.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Octet);
            requestContent.Headers.Add(CommonHeaders.HashHeaderName, objectHash.ToString());

            {
                HttpResponseMessage result = await Client!.PutAsync(new Uri($"api/v1/refs/{TestNamespace}/{bucket}/{existingObject}", UriKind.Relative), requestContent);
                result.EnsureSuccessStatusCode();
            }

            IoHashKey missingObject = IoHashKey.FromName("missingObject");

            string queryString = $"?names={bucket}.{existingObject}&names={bucket}.{missingObject}";

            {
                HttpResponseMessage result = await Client!.GetAsync(new Uri($"api/v1/refs/{TestNamespace}/exists" + queryString, UriKind.Relative));
                result.EnsureSuccessStatusCode();
                ExistCheckMultipleRefsResponse response = await result.Content.ReadAsAsync<ExistCheckMultipleRefsResponse>();

                Assert.AreEqual(1, response.Missing.Count);
                Assert.AreEqual(bucket, response.Missing[0].Bucket);
                Assert.AreEqual(missingObject, response.Missing[0].Key);
            }
        }

        [TestMethod]
        public async Task PutGetObjectHierarchy()
        {
            string blobContents = "This is a string that is referenced as a blob";
            byte[] blobData = Encoding.ASCII.GetBytes(blobContents);
            BlobIdentifier blobHash = BlobIdentifier.FromBlob(blobData);
            await Service.PutObject(TestNamespace, blobData, blobHash);

            string blobContentsChild = "This string is also referenced as a blob but from a child object";
            byte[] dataChild = Encoding.ASCII.GetBytes(blobContentsChild);
            BlobIdentifier blobHashChild = BlobIdentifier.FromBlob(dataChild);
            await Service.PutObject(TestNamespace, dataChild, blobHashChild);

            CbWriter writerChild = new CbWriter();
            writerChild.BeginObject();
            writerChild.WriteBinaryAttachment("blob", blobHashChild.AsIoHash());
            writerChild.EndObject();

            byte[] childDataObject = writerChild.ToByteArray();
            BlobIdentifier childDataObjectHash = BlobIdentifier.FromBlob(childDataObject);
            await Service.PutObject(TestNamespace, childDataObject, childDataObjectHash);

            CbWriter writerParent = new CbWriter();
            writerParent.BeginObject();
            
            writerParent.WriteBinaryAttachment("blobAttachment", blobHash.AsIoHash());
            writerParent.WriteObjectAttachment("objectAttachment", childDataObjectHash.AsIoHash());
            writerParent.EndObject();

            byte[] objectData = writerParent.ToByteArray();
            BlobIdentifier objectHash = BlobIdentifier.FromBlob(objectData);

            IoHashKey key = IoHashKey.FromName("newHierarchyObject");

            using HttpContent requestContent = new ByteArrayContent(objectData);
            requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);
            requestContent.Headers.Add(CommonHeaders.HashHeaderName, objectHash.ToString());

            HttpResponseMessage result = await Client!.PutAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}.uecb", UriKind.Relative), requestContent);
            result.EnsureSuccessStatusCode();

            // check the response
            {
                Assert.AreEqual(result!.Content.Headers.ContentType!.MediaType, CustomMediaTypeNames.UnrealCompactBinary);
                // check that no blobs are missing
                await using MemoryStream ms = new MemoryStream();
                await result.Content.CopyToAsync(ms);
                byte[] roundTrippedBuffer = ms.ToArray();
                CbObject cb = new CbObject(roundTrippedBuffer);
                CbField needsField = cb["needs"];
                List<BlobIdentifier> missingBlobs = needsField.AsArray().Select(field => BlobIdentifier.FromIoHash(field.AsHash())).ToList();
                Assert.AreEqual(0, missingBlobs.Count);
            }

            // check that actual internal representation
            {
                BucketId bucket = new BucketId("bucket");

                ObjectRecord objectRecord = await ReferencesStore.Get(TestNamespace, bucket, key, IReferencesStore.FieldFlags.IncludePayload);

                Assert.IsTrue(objectRecord.IsFinalized);
                Assert.AreEqual(key, objectRecord.Name);
                Assert.AreEqual(objectHash, objectRecord.BlobIdentifier);
                Assert.IsNotNull(objectRecord.InlinePayload);
            }

            // verify attachments
            {
                HttpResponseMessage getResponse = await Client.GetAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}.raw", UriKind.Relative));
                getResponse.EnsureSuccessStatusCode();
                await using MemoryStream ms = new MemoryStream();
                await getResponse.Content.CopyToAsync(ms);

                byte[] roundTrippedBuffer = ms.ToArray();

                CollectionAssert.AreEqual(objectData, roundTrippedBuffer);
                Assert.AreEqual(objectHash, BlobIdentifier.FromBlob(roundTrippedBuffer));
            }

            {
                BlobIdentifier blobAttachment;
                BlobIdentifier objectAttachment;
                {
                    HttpResponseMessage getResponse = await Client.GetAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}.uecb", UriKind.Relative));
                    getResponse.EnsureSuccessStatusCode();
                    await using MemoryStream ms = new MemoryStream();
                    await getResponse.Content.CopyToAsync(ms);

                    byte[] roundTrippedBuffer = ms.ToArray();
                    ReadOnlyMemory<byte> localMemory = new ReadOnlyMemory<byte>(roundTrippedBuffer);
                    CbObject cb = new CbObject(roundTrippedBuffer);
                    Assert.AreEqual(2, cb.Count());

                    CbField blobAttachmentField = cb["blobAttachment"];
                    Assert.AreNotEqual(CbField.Empty, blobAttachmentField);
                    blobAttachment = BlobIdentifier.FromIoHash(blobAttachmentField.AsBinaryAttachment());
                    CbField objectAttachmentField = cb["objectAttachment"];
                    Assert.AreNotEqual(CbField.Empty, objectAttachmentField);
                    objectAttachment = BlobIdentifier.FromIoHash(objectAttachmentField.AsObjectAttachment().Hash);
                }

                {
                    HttpResponseMessage getAttachment = await Client.GetAsync(new Uri($"api/v1/blobs/{TestNamespace}/{blobAttachment}", UriKind.Relative));
                    getAttachment.EnsureSuccessStatusCode();
                    await using MemoryStream ms = new MemoryStream();
                    await getAttachment.Content.CopyToAsync(ms);
                    byte[] roundTrippedBuffer = ms.ToArray();
                    string roundTrippedString = Encoding.ASCII.GetString(roundTrippedBuffer);

                    Assert.AreEqual(blobContents, roundTrippedString);
                    Assert.AreEqual(blobHash, BlobIdentifier.FromBlob(roundTrippedBuffer));
                }

                BlobIdentifier attachedBlobIdentifier;
                {
                    HttpResponseMessage getAttachment = await Client.GetAsync(new Uri($"api/v1/blobs/{TestNamespace}/{objectAttachment}", UriKind.Relative));
                    getAttachment.EnsureSuccessStatusCode();
                    await using MemoryStream ms = new MemoryStream();
                    await getAttachment.Content.CopyToAsync(ms);
                    byte[] roundTrippedBuffer = ms.ToArray();
                    ReadOnlyMemory<byte> localMemory = new ReadOnlyMemory<byte>(roundTrippedBuffer);
                    CbObject cb = new CbObject(roundTrippedBuffer);
                    Assert.AreEqual(1, cb.Count());

                    CbField blobField = cb["blob"];
                    Assert.AreNotEqual(CbField.Empty, blobField);

                    attachedBlobIdentifier = BlobIdentifier.FromIoHash(blobField!.AsBinaryAttachment());
                }

                {
                    HttpResponseMessage getAttachment = await Client.GetAsync(new Uri($"api/v1/blobs/{TestNamespace}/{attachedBlobIdentifier}", UriKind.Relative));
                    getAttachment.EnsureSuccessStatusCode();
                    await using MemoryStream ms = new MemoryStream();
                    await getAttachment.Content.CopyToAsync(ms);
                    byte[] roundTrippedBuffer = ms.ToArray();
                    string roundTrippedString = Encoding.ASCII.GetString(roundTrippedBuffer);

                    Assert.AreEqual(blobContentsChild, roundTrippedString);
                    Assert.AreEqual(blobHashChild, BlobIdentifier.FromBlob(roundTrippedBuffer));
                }
            }

            // check json representation
            {
                HttpResponseMessage getResponse = await Client.GetAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}.json", UriKind.Relative));
                getResponse.EnsureSuccessStatusCode();
                await using MemoryStream ms = new MemoryStream();
                await getResponse.Content.CopyToAsync(ms);

                byte[] roundTrippedBuffer = ms.ToArray();
                string s = Encoding.ASCII.GetString(roundTrippedBuffer);
                JObject jObject = JObject.Parse(s);
                Assert.AreEqual(2, jObject.Children().Count());

                JToken? blobAttachment = jObject["blobAttachment"];
                string blobAttachmentString = blobAttachment!.Value<string>()!;
                Assert.AreEqual(blobHash, new BlobIdentifier(blobAttachmentString));

                JToken? objectAttachment = jObject["objectAttachment"];
                string objectAttachmentString = objectAttachment!.Value<string>()!;
                Assert.AreEqual(childDataObjectHash, new BlobIdentifier(objectAttachmentString));

            }
        }
        
        [TestMethod]
        public async Task PutPartialHierarchy()
        {
            // do not submit the content of the blobs, which should be reported in the response of the put
            string blobContents = "This is a string that is referenced as a blob";
            byte[] blobData = Encoding.ASCII.GetBytes(blobContents);
            BlobIdentifier blobHash = BlobIdentifier.FromBlob(blobData);

            string blobContentsChild = "This string is also referenced as a blob but from a child object";
            byte[] dataChild = Encoding.ASCII.GetBytes(blobContentsChild);
            BlobIdentifier blobHashChild = BlobIdentifier.FromBlob(dataChild);

            CbWriter writerChild = new CbWriter();
            writerChild.BeginObject();
            writerChild.WriteBinaryAttachment("blob", blobHashChild.AsIoHash());
            writerChild.EndObject();

            byte[] childDataObject = writerChild.ToByteArray();
            BlobIdentifier childDataObjectHash = BlobIdentifier.FromBlob(childDataObject);
            await Service.PutObject(TestNamespace, childDataObject, childDataObjectHash);

            CbWriter writerParent = new CbWriter();
            writerParent.BeginObject();
            
            writerParent.WriteBinaryAttachment("blobAttachment", blobHash.AsIoHash());
            writerParent.WriteObjectAttachment("objectAttachment", childDataObjectHash.AsIoHash());
            writerParent.EndObject();

            byte[] objectData = writerParent.ToByteArray();
            BlobIdentifier objectHash = BlobIdentifier.FromBlob(objectData);
            
            IoHashKey key = IoHashKey.FromName("newHierarchyObject");

            {
                using HttpContent requestContent = new ByteArrayContent(objectData);
                requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);
                requestContent.Headers.Add(CommonHeaders.HashHeaderName, objectHash.ToString());

                HttpResponseMessage result = await Client!.PutAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}.uecb", UriKind.Relative), requestContent);
                result.EnsureSuccessStatusCode();

                {
                    Assert.AreEqual(result!.Content.Headers.ContentType!.MediaType, CustomMediaTypeNames.UnrealCompactBinary);
                    await using MemoryStream ms = new MemoryStream();
                    await result.Content.CopyToAsync(ms);
                    byte[] roundTrippedBuffer = ms.ToArray();
                    ReadOnlyMemory<byte> localMemory = new ReadOnlyMemory<byte>(roundTrippedBuffer);
                    CbObject cb = new CbObject(roundTrippedBuffer);
                    CbField needsField = cb["needs"];
                    List<BlobIdentifier> missingBlobs = needsField.AsArray().Select(field => BlobIdentifier.FromIoHash(field.AsHash())).ToList();
                    Assert.AreEqual(2, missingBlobs.Count);
                    Assert.IsTrue(missingBlobs.Contains(blobHash));
                    Assert.IsTrue(missingBlobs.Contains(blobHashChild));
                }
            }

            {
                using HttpContent requestContent = new ByteArrayContent(objectData);
                requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);
                requestContent.Headers.Add(CommonHeaders.HashHeaderName, objectHash.ToString());

                HttpResponseMessage result = await Client!.PutAsync( new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}.json", UriKind.Relative), requestContent);
                result.EnsureSuccessStatusCode();

                {
                    Assert.AreEqual(result!.Content.Headers.ContentType!.MediaType, MediaTypeNames.Application.Json);
                    await using MemoryStream ms = new MemoryStream();
                    await result.Content.CopyToAsync(ms);
                    byte[] roundTrippedBuffer = ms.ToArray();
                    string s = Encoding.ASCII.GetString(roundTrippedBuffer);
                    JObject jObject = JObject.Parse(s);
                    Assert.AreEqual(1, jObject.Children().Count());

                    JToken? needs = jObject["needs"];
                    BlobIdentifier[] missingBlobs = needs!.ToArray().Select(token => new BlobIdentifier(token.Value<string>()!)).ToArray();
                    Assert.AreEqual(2, missingBlobs.Length);
                    Assert.IsTrue(missingBlobs.Contains(blobHash));
                    Assert.IsTrue(missingBlobs.Contains(blobHashChild));
                }
            }
        }

        
        [TestMethod]
        public async Task PutContentIdMissingBlob()
        {
            IContentIdStore? contentIdStore = Server!.Services.GetService<IContentIdStore>();
            Assert.IsNotNull(contentIdStore);

            // submit a object which contains a content id, which exists but points to a blob that does not exist
            string blobContents = "This is a string that is referenced as a blob";
            byte[] blobData = Encoding.ASCII.GetBytes(blobContents);
            BlobIdentifier blobHash = BlobIdentifier.FromBlob(blobData);
            ContentId contentId = new ContentId("0000000000000000000000000000000000000000");

            await contentIdStore.Put(TestNamespace, contentId, blobHash, blobData.Length);

            CbWriter writer = new CbWriter();
            writer.BeginObject();
            writer.WriteBinaryAttachment("blob", contentId.AsIoHash());
            writer.EndObject();

            byte[] objectData = writer.ToByteArray();
            BlobIdentifier objectHash = BlobIdentifier.FromBlob(objectData);
            
            IoHashKey key = IoHashKey.FromName("putContentIdMissingBlob");

            {
                using HttpContent requestContent = new ByteArrayContent(objectData);
                requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);
                requestContent.Headers.Add(CommonHeaders.HashHeaderName, objectHash.ToString());

                HttpResponseMessage result = await Client!.PutAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}.uecb", UriKind.Relative), requestContent);
                result.EnsureSuccessStatusCode();

                {
                    Assert.AreEqual(result!.Content.Headers.ContentType!.MediaType, CustomMediaTypeNames.UnrealCompactBinary);
                    await using MemoryStream ms = new MemoryStream();
                    await result.Content.CopyToAsync(ms);
                    byte[] roundTrippedBuffer = ms.ToArray();
                    CbObject cb = new CbObject(roundTrippedBuffer);
                    CbField needsField = cb["needs"];
                    Assert.AreNotEqual(CbField.Empty, needsField);
                    List<IoHash> missingBlobs = needsField.AsArray().Select(field => field.AsHash()).ToList();
                    Assert.AreEqual(1, missingBlobs.Count);

                    Assert.AreNotEqual(blobHash, missingBlobs[0], "Refs should not be returning the mapped blob identifiers as this is unknown to the client attempting to put a new ref");
                    Assert.AreEqual(contentId.AsBlobIdentifier(), BlobIdentifier.FromIoHash(missingBlobs[0]));
                }
            }

            {
                using HttpContent requestContent = new ByteArrayContent(objectData);
                requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);
                requestContent.Headers.Add(CommonHeaders.HashHeaderName, objectHash.ToString());

                HttpResponseMessage result = await Client!.PutAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}.json", UriKind.Relative), requestContent);
                result.EnsureSuccessStatusCode();

                {
                    Assert.AreEqual(result!.Content.Headers.ContentType!.MediaType, MediaTypeNames.Application.Json);
                    await using MemoryStream ms = new MemoryStream();
                    await result.Content.CopyToAsync(ms);
                    byte[] roundTrippedBuffer = ms.ToArray();
                    string s = Encoding.ASCII.GetString(roundTrippedBuffer);
                    JObject jObject = JObject.Parse(s);
                    Assert.AreEqual(1, jObject.Children().Count());

                    JToken? needs = jObject["needs"];
                    BlobIdentifier[] missingBlobs = needs!.ToArray().Select(token => new BlobIdentifier(token.Value<string>()!)).ToArray();
                    Assert.AreEqual(1, missingBlobs.Length);
                    Assert.AreEqual(contentId.AsBlobIdentifier(), missingBlobs[0]);
                }
            }
        }

        
        [TestMethod]
        public async Task PutMissingAttachmentComplex()
        {
            string blobContents = "This is a string that is referenced as a blob but will not be uploaded";
            byte[] blobData = Encoding.ASCII.GetBytes(blobContents);
            BlobIdentifier blobHash = BlobIdentifier.FromBlob(blobData);

            CbWriter writer = new CbWriter();
            writer.BeginObject();
            writer.WriteString("name", "randomStringContent");
            writer.BeginArray("values");
            writer.BeginObject();
            writer.WriteInteger("rawSize", 200);
            writer.WriteBinaryAttachment("rawHash", blobHash.AsIoHash());
            writer.EndObject();
            writer.EndArray();
            writer.EndObject();

            byte[] objectData = writer.ToByteArray();
            BlobIdentifier objectHash = BlobIdentifier.FromBlob(objectData);

            IoHashKey key = IoHashKey.FromName("putContentIdMissingBlobComplex");

            {
                using HttpContent requestContent = new ByteArrayContent(objectData);
                requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);
                requestContent.Headers.Add(CommonHeaders.HashHeaderName, objectHash.ToString());

                HttpResponseMessage result = await Client!.PutAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}.uecb", UriKind.Relative), requestContent);
                result.EnsureSuccessStatusCode();

                {
                    Assert.AreEqual(result!.Content.Headers.ContentType!.MediaType, CustomMediaTypeNames.UnrealCompactBinary);
                    await using MemoryStream ms = new MemoryStream();
                    await result.Content.CopyToAsync(ms);
                    byte[] roundTrippedBuffer = ms.ToArray();
                    CbObject cb = new CbObject(roundTrippedBuffer);
                    CbField needsField = cb["needs"];
                    Assert.AreNotEqual(CbField.Empty, needsField);
                    List<IoHash> missingBlobs = needsField.AsArray().Select(field => field.AsHash()).ToList();
                    Assert.AreEqual(1, missingBlobs.Count);

                    Assert.AreEqual(blobHash.AsIoHash(), missingBlobs[0], "Expected to find missing hash even for attachments not directly in the root object");
                }
            }
        }

        [TestMethod]
        public async Task PutAndFinalize()
        {
            BucketId bucket = new BucketId("bucket");
            IoHashKey key = IoHashKey.FromName("willFinalizeObject");

            // do not submit the content of the blobs, which should be reported in the response of the put
            string blobContents = "This is a string that is referenced as a blob";
            byte[] blobData = Encoding.ASCII.GetBytes(blobContents);
            BlobIdentifier blobHash = BlobIdentifier.FromBlob(blobData);

            string blobContentsChild = "This string is also referenced as a blob but from a child object";
            byte[] dataChild = Encoding.ASCII.GetBytes(blobContentsChild);
            BlobIdentifier blobHashChild = BlobIdentifier.FromBlob(dataChild);

            CbWriter writerChild = new CbWriter();
            writerChild.BeginObject();
            writerChild.WriteBinaryAttachment("blob", blobHashChild.AsIoHash());
            writerChild.EndObject();

            byte[] childDataObject = writerChild.ToByteArray();
            BlobIdentifier childDataObjectHash = BlobIdentifier.FromBlob(childDataObject);
            await Service.PutObject(TestNamespace, childDataObject, childDataObjectHash);

            CbWriter writerParent = new CbWriter();
            writerParent.BeginObject();
            
            writerParent.WriteBinaryAttachment("blobAttachment", blobHash.AsIoHash());
            writerParent.WriteObjectAttachment("objectAttachment", childDataObjectHash.AsIoHash());
            writerParent.EndObject();

            byte[] objectData = writerParent.ToByteArray();
            BlobIdentifier objectHash = BlobIdentifier.FromBlob(objectData);

            {
                using HttpContent requestContent = new ByteArrayContent(objectData);
                requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);
                requestContent.Headers.Add(CommonHeaders.HashHeaderName, objectHash.ToString());

                HttpResponseMessage result = await Client!.PutAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}.uecb", UriKind.Relative), requestContent);
                result.EnsureSuccessStatusCode();

                {
                    Assert.AreEqual(result!.Content.Headers.ContentType!.MediaType, CustomMediaTypeNames.UnrealCompactBinary);
                    await using MemoryStream ms = new MemoryStream();
                    await result.Content.CopyToAsync(ms);
                    byte[] roundTrippedBuffer = ms.ToArray();
                    CbObject cb = new CbObject(roundTrippedBuffer);
                    CbField needsField = cb["needs"];
                    List<BlobIdentifier> missingBlobs = needsField.AsArray().Select(field => BlobIdentifier.FromIoHash(field.AsHash())).ToList();
                    Assert.AreEqual(2, missingBlobs.Count);
                    Assert.IsTrue(missingBlobs.Contains(blobHash));
                    Assert.IsTrue(missingBlobs.Contains(blobHashChild));
                }
            }

            // check that actual internal representation
            {
                ObjectRecord objectRecord = await ReferencesStore.Get(TestNamespace, bucket, key, IReferencesStore.FieldFlags.IncludePayload);

                Assert.IsFalse(objectRecord.IsFinalized);
                Assert.AreEqual(key, objectRecord.Name);
                Assert.AreEqual(objectHash, objectRecord.BlobIdentifier);
                Assert.IsNotNull(objectRecord.InlinePayload);
            }

            // upload missing pieces
            {
                await Service.PutObject(TestNamespace, blobData, blobHash);
                await Service.PutObject(TestNamespace, dataChild, blobHashChild);
            }

            // finalize the object as no pieces is now missing
            {
                using HttpContent requestContent = new ByteArrayContent(Array.Empty<byte>());

                HttpResponseMessage result = await Client!.PostAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}/finalize/{objectHash}.uecb", UriKind.Relative), requestContent);
                result.EnsureSuccessStatusCode();

                {
                    Assert.AreEqual(result!.Content.Headers.ContentType!.MediaType, CustomMediaTypeNames.UnrealCompactBinary);
                    await using MemoryStream ms = new MemoryStream();
                    await result.Content.CopyToAsync(ms);
                    byte[] roundTrippedBuffer = ms.ToArray();
                    ReadOnlyMemory<byte> localMemory = new ReadOnlyMemory<byte>(roundTrippedBuffer);
                    CbObject cb = new CbObject(roundTrippedBuffer);
                    CbField? needsField = cb["needs"];
                    List<BlobIdentifier> missingBlobs = needsField.AsArray().Select(field => BlobIdentifier.FromIoHash(field.AsHash())).ToList();
                    Assert.AreEqual(0, missingBlobs.Count);
                }
            }

            // check that actual internal representation has updated its state
            {
                ObjectRecord objectRecord = await ReferencesStore.Get(TestNamespace, bucket, key, IReferencesStore.FieldFlags.None);

                Assert.IsTrue(objectRecord.IsFinalized);
                Assert.AreEqual(key, objectRecord.Name);
                Assert.AreEqual(objectHash, objectRecord.BlobIdentifier);
            }
        }

        [TestMethod]
        public async Task GetMissingContentIdRecord()
        {
            string blobContents = "This is a blob";
            byte[] blobData = Encoding.ASCII.GetBytes(blobContents);
            BlobIdentifier uncompressedHash = BlobIdentifier.FromBlob(blobData);
            IoHashKey key = IoHashKey.FromName("compressedObject");

            CompressedBufferUtils bufferUtils = Server!.Services.GetService<CompressedBufferUtils>()!;
            byte[] compressedBuffer = bufferUtils.CompressContent(OoodleCompressorMethod.Mermaid, OoodleCompressionLevel.VeryFast, blobData);
            BlobIdentifier compressedHash = BlobIdentifier.FromBlob(compressedBuffer);

            CbObject cbObject = CbObject.Build(writer => writer.WriteBinaryAttachment("Attachment", uncompressedHash.AsIoHash()));
            byte[] cbObjectData = cbObject.GetView().ToArray();
            BlobIdentifier cbObjectHash = BlobIdentifier.FromBlob(cbObjectData);

            {
                // upload compressed blob
                using HttpContent requestContent = new ByteArrayContent(compressedBuffer);
                requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompressedBuffer);

                HttpResponseMessage result = await Client!.PutAsync(new Uri($"api/v1/compressed-blobs/{TestNamespace}/{uncompressedHash}", UriKind.Relative), requestContent);
                result.EnsureSuccessStatusCode();
            }

            {
                // upload ref
                using HttpContent requestContent = new ByteArrayContent(cbObjectData);
                requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);
                requestContent.Headers.Add(CommonHeaders.HashHeaderName, cbObjectHash.ToString());

                HttpResponseMessage result = await Client!.PutAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}.uecb", UriKind.Relative), requestContent);
                result.EnsureSuccessStatusCode();

                {
                    Assert.AreEqual(CustomMediaTypeNames.UnrealCompactBinary, result!.Content.Headers.ContentType!.MediaType);
                    // check that no blobs are missing
                    await using MemoryStream ms = new MemoryStream();
                    await result.Content.CopyToAsync(ms);
                    byte[] roundTrippedBuffer = ms.ToArray();
                    CbObject cb = new CbObject(roundTrippedBuffer);
                    CbField needsField = cb["needs"];
                    List<BlobIdentifier> missingBlobs = needsField.AsArray().Select(field => BlobIdentifier.FromIoHash(field.AsHash())).ToList();
                    Assert.AreEqual(0, missingBlobs.Count);
                }
            }

            {
                // verify we can fetch the blob properly
                HttpResponseMessage getResponse = await Client.GetAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}.uecb", UriKind.Relative));
                getResponse.EnsureSuccessStatusCode();
                await using MemoryStream ms = new MemoryStream();
                await getResponse.Content.CopyToAsync(ms);

                byte[] roundTrippedBuffer = ms.ToArray();

                CollectionAssert.AreEqual(cbObjectData, roundTrippedBuffer);
                Assert.AreEqual(cbObjectHash, BlobIdentifier.FromBlob(roundTrippedBuffer));
            }

            {
                // verify that head checks find the object
                using HttpRequestMessage headRequest = new HttpRequestMessage(HttpMethod.Head, new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}", UriKind.Relative));
                HttpResponseMessage getResponse = await Client.SendAsync(headRequest);
                getResponse.EnsureSuccessStatusCode();
            }

            {
                // verify that the exists check doesnt find any issue
                HttpResponseMessage existsResponse = await Client.GetAsync(new Uri($"api/v1/refs/{TestNamespace}/exists?names=bucket.{key}", UriKind.Relative));
                existsResponse.EnsureSuccessStatusCode();

                ExistCheckMultipleRefsResponse response = await existsResponse.Content.ReadAsAsync<ExistCheckMultipleRefsResponse>();
                Assert.AreEqual(0, response.Missing.Count);
            }

            {
                // delete the blob referenced by the compressed buffer
                await Service.DeleteObject(TestNamespace, compressedHash);
            }

            {
                // the compact binary object still exists, so this returns a success (trying to resolve the attachment will fail)
                HttpResponseMessage getResponse = await Client.GetAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}.uecb", UriKind.Relative));
                getResponse.EnsureSuccessStatusCode();
            }

            {
                // verify that head checks fail
                using HttpRequestMessage headRequest = new HttpRequestMessage(HttpMethod.Head, new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}", UriKind.Relative));
                HttpResponseMessage getResponse = await Client.SendAsync(headRequest);
                Assert.AreEqual(HttpStatusCode.NotFound, getResponse.StatusCode);
            }

            {
                // verify that the exists check correctly returns a missing blob
                HttpResponseMessage existsResponse = await Client.GetAsync(new Uri($"api/v1/refs/{TestNamespace}/exists?names=bucket.{key}", UriKind.Relative));
                existsResponse.EnsureSuccessStatusCode();

                ExistCheckMultipleRefsResponse response = await existsResponse.Content.ReadAsAsync<ExistCheckMultipleRefsResponse>();
                Assert.AreEqual(1, response.Missing.Count);
                Assert.AreEqual(key, response.Missing[0].Key);
                Assert.AreEqual("bucket", response.Missing[0].Bucket.ToString());
            }
        }

        [TestMethod]
        public async Task GetMissingCompressedBufferAttachment()
        {
            CbObject cbObjectAttachment = CbObject.Build(writer => writer.WriteString("ValueField", "This field has a value"));
            byte[] cbAttachmentData = cbObjectAttachment.GetView().ToArray();
            BlobIdentifier cbAttachmentHash = BlobIdentifier.FromBlob(cbAttachmentData);
            IoHashKey key = IoHashKey.FromName("compressedAttachedObject");

            CbObject cbObject = CbObject.Build(writer => writer.WriteObjectAttachment("Attachment", cbAttachmentHash.AsIoHash()));
            byte[] cbObjectData = cbObject.GetView().ToArray();
            BlobIdentifier cbObjectHash = BlobIdentifier.FromBlob(cbObjectData);

            {
                // upload compressed blob
                using HttpContent requestContent = new ByteArrayContent(cbAttachmentData);
                requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);

                HttpResponseMessage result = await Client!.PutAsync(new Uri($"api/v1/objects/{TestNamespace}/{cbAttachmentHash}", UriKind.Relative), requestContent);
                result.EnsureSuccessStatusCode();
            }

            {
                // upload ref
                using HttpContent requestContent = new ByteArrayContent(cbObjectData);
                requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);
                requestContent.Headers.Add(CommonHeaders.HashHeaderName, cbObjectHash.ToString());

                HttpResponseMessage result = await Client!.PutAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}.uecb", UriKind.Relative), requestContent);
                result.EnsureSuccessStatusCode();

                {
                    Assert.AreEqual(CustomMediaTypeNames.UnrealCompactBinary, result!.Content.Headers.ContentType!.MediaType);
                    // check that no blobs are missing
                    await using MemoryStream ms = new MemoryStream();
                    await result.Content.CopyToAsync(ms);
                    byte[] roundTrippedBuffer = ms.ToArray();
                    CbObject cb = new CbObject(roundTrippedBuffer);
                    CbField needsField = cb["needs"];
                    List<BlobIdentifier> missingBlobs = needsField.AsArray().Select(field => BlobIdentifier.FromIoHash(field.AsHash())).ToList();
                    Assert.AreEqual(0, missingBlobs.Count);
                }
            }

            {
                // verify we can fetch the blob properly
                HttpResponseMessage getResponse = await Client.GetAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}.uecb", UriKind.Relative));
                getResponse.EnsureSuccessStatusCode();
                await using MemoryStream ms = new MemoryStream();
                await getResponse.Content.CopyToAsync(ms);

                byte[] roundTrippedBuffer = ms.ToArray();

                CollectionAssert.AreEqual(cbObjectData, roundTrippedBuffer);
                Assert.AreEqual(cbObjectHash, BlobIdentifier.FromBlob(roundTrippedBuffer));
            }

            {
                // verify that head checks find the object
                using HttpRequestMessage headRequest = new HttpRequestMessage(HttpMethod.Head, new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}", UriKind.Relative));
                HttpResponseMessage getResponse = await Client.SendAsync(headRequest);
                getResponse.EnsureSuccessStatusCode();
            }

            {
                // verify that the exists check doesnt find any issue
                HttpResponseMessage existsResponse = await Client.GetAsync(new Uri($"api/v1/refs/{TestNamespace}/exists?names=bucket.{key}", UriKind.Relative));
                existsResponse.EnsureSuccessStatusCode();

                ExistCheckMultipleRefsResponse response = await existsResponse.Content.ReadAsAsync<ExistCheckMultipleRefsResponse>();
                Assert.AreEqual(0, response.Missing.Count);
            }

            {
                // delete the blob referenced by the compressed buffer
                await Service.DeleteObject(TestNamespace, cbAttachmentHash);
            }

            {
                // the compact binary object still exists, so this returns a success (trying to resolve the attachment will fail)
                HttpResponseMessage getResponse = await Client.GetAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}.uecb", UriKind.Relative));
                getResponse.EnsureSuccessStatusCode();
            }

            {
                // verify that head checks fail
                using HttpRequestMessage headRequest = new HttpRequestMessage(HttpMethod.Head, new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}", UriKind.Relative));
                HttpResponseMessage getResponse = await Client.SendAsync(headRequest);
                Assert.AreEqual(HttpStatusCode.NotFound, getResponse.StatusCode);
            }

            {
                // verify that the exists check correctly returns a missing blob
                HttpResponseMessage existsResponse = await Client.GetAsync(new Uri($"api/v1/refs/{TestNamespace}/exists?names=bucket.{key}", UriKind.Relative));
                existsResponse.EnsureSuccessStatusCode();

                ExistCheckMultipleRefsResponse response = await existsResponse.Content.ReadAsAsync<ExistCheckMultipleRefsResponse>();
                Assert.AreEqual(1, response.Missing.Count);
                Assert.AreEqual(key, response.Missing[0].Key);
                Assert.AreEqual("bucket", response.Missing[0].Bucket.ToString());
            }
        }

        [TestMethod]
        public async Task GetMissingBlobRecord()
        {
            string blobContents = "This is a blob";
            byte[] blobData = Encoding.ASCII.GetBytes(blobContents);
            BlobIdentifier blobHash = BlobIdentifier.FromBlob(blobData);
            IoHashKey key = IoHashKey.FromName("newReferenceObject");

            using HttpContent requestContent = new ByteArrayContent(blobData);
            requestContent.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Octet);
            requestContent.Headers.Add(CommonHeaders.HashHeaderName, blobHash.ToString());

            HttpResponseMessage result = await Client!.PutAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}.uecb", UriKind.Relative), requestContent);
            result.EnsureSuccessStatusCode();

            {
                Assert.AreEqual(CustomMediaTypeNames.UnrealCompactBinary, result!.Content.Headers.ContentType!.MediaType);
                // check that no blobs are missing
                await using MemoryStream ms = new MemoryStream();
                await result.Content.CopyToAsync(ms);
                byte[] roundTrippedBuffer = ms.ToArray();
                CbObject cb = new CbObject(roundTrippedBuffer);
                CbField needsField = cb["needs"];
                List<BlobIdentifier> missingBlobs = needsField.AsArray().Select(field => BlobIdentifier.FromIoHash(field.AsHash())).ToList();
                Assert.AreEqual(0, missingBlobs.Count);
            }

            {
                // verify we can fetch the blob properly
                HttpResponseMessage getResponse = await Client.GetAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}.raw", UriKind.Relative));
                getResponse.EnsureSuccessStatusCode();
                await using MemoryStream ms = new MemoryStream();
                await getResponse.Content.CopyToAsync(ms);

                byte[] roundTrippedBuffer = ms.ToArray();

                CollectionAssert.AreEqual(blobData, roundTrippedBuffer);
                Assert.AreEqual(blobHash, BlobIdentifier.FromBlob(roundTrippedBuffer));
            }

            {
                // verify that head checks find the object
                using HttpRequestMessage headRequest = new HttpRequestMessage(HttpMethod.Head, new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}", UriKind.Relative));
                HttpResponseMessage getResponse = await Client.SendAsync(headRequest);
                getResponse.EnsureSuccessStatusCode();
            }

            {
                // verify that the exists check doesnt find any issue
                HttpResponseMessage existsResponse = await Client.GetAsync(new Uri($"api/v1/refs/{TestNamespace}/exists?names=bucket.{key}", UriKind.Relative));
                existsResponse.EnsureSuccessStatusCode();

                ExistCheckMultipleRefsResponse response = await existsResponse.Content.ReadAsAsync<ExistCheckMultipleRefsResponse>();
                Assert.AreEqual(0, response.Missing.Count);
            }

            {
                // delete the blob 
                await Service.DeleteObject(TestNamespace, blobHash);
            }

            {
                // the compact binary object still exists, so this returns a success (trying to resolve the attachment will fail)
                HttpResponseMessage getResponse = await Client.GetAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}.uecb", UriKind.Relative));
                getResponse.EnsureSuccessStatusCode();
            }

            {
                // the compact binary object still exists, so this returns a success (trying to resolve the attachment will fail)
                HttpResponseMessage getResponse = await Client.GetAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}.json", UriKind.Relative));
                getResponse.EnsureSuccessStatusCode();
            }

            {
                // we should now see a 404 as the blob is missing
                HttpResponseMessage getResponse = await Client.GetAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}.raw", UriKind.Relative));
                Assert.AreEqual(HttpStatusCode.NotFound, getResponse.StatusCode);
                Assert.AreEqual("application/problem+json", getResponse.Content.Headers.ContentType!.MediaType);
                string s = await getResponse.Content.ReadAsStringAsync();
                ProblemDetails? problem = JsonConvert.DeserializeObject<ProblemDetails>(s);
                Assert.IsNotNull(problem);
                Assert.AreEqual($"Object {blobHash} in {TestNamespace} not found", problem.Title);
            }

            {
                // verify that head checks fail
                using HttpRequestMessage headRequest = new HttpRequestMessage(HttpMethod.Head, new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}", UriKind.Relative));
                HttpResponseMessage getResponse = await Client.SendAsync(headRequest);
                Assert.AreEqual(HttpStatusCode.NotFound, getResponse.StatusCode);
            }

            {
                // verify that the exists check correctly returns a missing blob
                HttpResponseMessage existsResponse = await Client.GetAsync(new Uri($"api/v1/refs/{TestNamespace}/exists?names=bucket.{key}", UriKind.Relative));
                existsResponse.EnsureSuccessStatusCode();

                ExistCheckMultipleRefsResponse response = await existsResponse.Content.ReadAsAsync<ExistCheckMultipleRefsResponse>();
                Assert.AreEqual(1, response.Missing.Count);
                Assert.AreEqual(key, response.Missing[0].Key);
                Assert.AreEqual("bucket", response.Missing[0].Bucket.ToString());
            }
        }

        [TestMethod]
        public async Task DeleteObject()
        {
            const string ObjectContents = "This is treated as a opaque blob";
            byte[] data = Encoding.ASCII.GetBytes(ObjectContents);
            BlobIdentifier objectHash = BlobIdentifier.FromBlob(data);

            using HttpContent requestContent = new ByteArrayContent(data);
            requestContent.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Octet);
            requestContent.Headers.Add(CommonHeaders.HashHeaderName, objectHash.ToString());

            IoHashKey key = IoHashKey.FromName("deletableObject");
            // submit the object
            {
                HttpResponseMessage result = await Client!.PutAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}", UriKind.Relative), requestContent);
                result.EnsureSuccessStatusCode();
            }

            // verify it is present
            {
                HttpResponseMessage result = await Client!.GetAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}.raw", UriKind.Relative));
                result.EnsureSuccessStatusCode();
            }

            // delete the object
            {
                HttpResponseMessage result = await Client!.DeleteAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}", UriKind.Relative));
                result.EnsureSuccessStatusCode();
            }

            // ensure the object is not present anymore
            {
                HttpResponseMessage result = await Client!.GetAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}.raw", UriKind.Relative));
                Assert.AreEqual(HttpStatusCode.NotFound, result.StatusCode);
            }

            // delete the object again which doesn't exist anymore
            {
                HttpResponseMessage result = await Client!.DeleteAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}", UriKind.Relative));
                Assert.AreEqual(HttpStatusCode.NotFound, result.StatusCode);
            }
        }

        
        [TestMethod]
        public async Task DropBucket()
        {
            const string BucketToDelete = "delete-bucket";

            const string ObjectContents = "This is treated as a opaque blob";
            byte[] data = Encoding.ASCII.GetBytes(ObjectContents);
            BlobIdentifier objectHash = BlobIdentifier.FromBlob(data);

            using HttpContent requestContent = new ByteArrayContent(data);
            requestContent.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Octet);
            requestContent.Headers.Add(CommonHeaders.HashHeaderName, objectHash.ToString());

            IoHashKey key = IoHashKey.FromName("deletableObject");

            // submit the object into multiple buckets
            {
                HttpResponseMessage result = await Client!.PutAsync(new Uri($"api/v1/refs/{TestNamespace}/{BucketToDelete}/{key}", UriKind.Relative), requestContent);
                result.EnsureSuccessStatusCode();
            }

            {
                HttpResponseMessage result = await Client!.PutAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}", UriKind.Relative), requestContent);
                result.EnsureSuccessStatusCode();
            }

            // verify it is present
            {
                HttpResponseMessage result = await Client!.GetAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}.raw", UriKind.Relative));
                result.EnsureSuccessStatusCode();
            }

            // verify it is present
            {
                HttpResponseMessage result = await Client!.GetAsync(new Uri($"api/v1/refs/{TestNamespace}/{BucketToDelete}/{key}.raw", UriKind.Relative));
                result.EnsureSuccessStatusCode();
            }

            // delete the bucket
            {
                HttpResponseMessage result = await Client!.DeleteAsync(new Uri($"api/v1/refs/{TestNamespace}/{BucketToDelete}", UriKind.Relative));
                result.EnsureSuccessStatusCode();
            }

            // ensure the object is present in not deleted bucket
            {
                HttpResponseMessage result = await Client!.GetAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}.raw", UriKind.Relative));
                result.EnsureSuccessStatusCode();
            }

            // ensure the object is not present anymore
            {
                HttpResponseMessage result = await Client!.GetAsync(new Uri($"api/v1/refs/{TestNamespace}/{BucketToDelete}/{key}.raw", UriKind.Relative));
                Assert.AreEqual(HttpStatusCode.NotFound, result.StatusCode);
            }
        }

        
        [TestMethod]
        public async Task DeleteNamespace()
        {
            const string NamespaceToBeDeleted = "test-delete-namespace";

            const string ObjectContents = "This is treated as a opaque blob";
            byte[] data = Encoding.ASCII.GetBytes(ObjectContents);
            BlobIdentifier objectHash = BlobIdentifier.FromBlob(data);

            using HttpContent requestContent = new ByteArrayContent(data);
            requestContent.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Octet);
            requestContent.Headers.Add(CommonHeaders.HashHeaderName, objectHash.ToString());

            IoHashKey key = IoHashKey.FromName("deletableObject");
            // submit the object into multiple namespaces
            {
                HttpResponseMessage result = await Client!.PutAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}", UriKind.Relative), requestContent);
                result.EnsureSuccessStatusCode();
            }

            {
                HttpResponseMessage result = await Client!.PutAsync(new Uri($"api/v1/refs/{NamespaceToBeDeleted}/bucket/{key}", UriKind.Relative), requestContent);
                result.EnsureSuccessStatusCode();
            }

            // verify it is present
            {
                HttpResponseMessage result = await Client!.GetAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}.raw", UriKind.Relative));
                result.EnsureSuccessStatusCode();
            }

            // verify it is present
            {
                HttpResponseMessage result = await Client!.GetAsync(new Uri($"api/v1/refs/{NamespaceToBeDeleted}/bucket/{key}.raw", UriKind.Relative));
                result.EnsureSuccessStatusCode();
            }

            // delete the namespace
            {
                HttpResponseMessage result = await Client!.DeleteAsync(new Uri($"api/v1/refs/{NamespaceToBeDeleted}", UriKind.Relative));
                result.EnsureSuccessStatusCode();
            }

            // ensure the object is present in not deleted namespace
            {
                HttpResponseMessage result = await Client!.GetAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}.raw", UriKind.Relative));
                result.EnsureSuccessStatusCode();
            }

            // ensure the object is not present anymore
            {
                HttpResponseMessage result = await Client!.GetAsync(new Uri($"api/v1/refs/{NamespaceToBeDeleted}/bucket/{key}.raw", UriKind.Relative));
                Assert.AreEqual(HttpStatusCode.NotFound, result.StatusCode);
            }

            // make sure the namespace is not considered a valid namespace anymore
            {
                HttpResponseMessage result = await Client!.GetAsync(new Uri($"api/v1/refs", UriKind.Relative));
                result.EnsureSuccessStatusCode();
                Assert.AreEqual(result!.Content.Headers.ContentType!.MediaType, MediaTypeNames.Application.Json);
                GetNamespacesResponse response = await result.Content.ReadAsAsync<GetNamespacesResponse>();
                CollectionAssert.DoesNotContain(response.Namespaces, NamespaceToBeDeleted);
            }
        }

        
        [TestMethod]
        public async Task ListNamespaces()
        {
            const string ObjectContents = "This is treated as a opaque blob";
            byte[] data = Encoding.ASCII.GetBytes(ObjectContents);
            BlobIdentifier objectHash = BlobIdentifier.FromBlob(data);
            IoHashKey key = IoHashKey.FromName("notUsedObject");

            using HttpContent requestContent = new ByteArrayContent(data);
            requestContent.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Octet);
            requestContent.Headers.Add(CommonHeaders.HashHeaderName, objectHash.ToString());

            // submit a object to make sure a namespace is created
            {
                HttpResponseMessage result = await Client!.PutAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}", UriKind.Relative), requestContent);
                result.EnsureSuccessStatusCode();
            }

            {
                HttpResponseMessage result = await Client!.GetAsync(new Uri($"api/v1/refs", UriKind.Relative));
                result.EnsureSuccessStatusCode();
                Assert.AreEqual(result!.Content.Headers.ContentType!.MediaType, MediaTypeNames.Application.Json);
                GetNamespacesResponse response = await result.Content.ReadAsAsync<GetNamespacesResponse>();
                Assert.IsTrue(response.Namespaces.Contains(TestNamespace));
            }
        }
        
        [TestMethod]
        public async Task GetOldRecords()
        {
            const string ObjectContents = "This is treated as a opaque blob";
            byte[] data = Encoding.ASCII.GetBytes(ObjectContents);
            BlobIdentifier objectHash = BlobIdentifier.FromBlob(data);

            using HttpContent requestContent = new ByteArrayContent(data);
            requestContent.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Octet);
            requestContent.Headers.Add(CommonHeaders.HashHeaderName, objectHash.ToString());

            IoHashKey key = IoHashKey.FromName("oldRecord");
            // submit some contents
            {
                HttpResponseMessage result = await Client!.PutAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}", UriKind.Relative), requestContent);
                result.EnsureSuccessStatusCode();
            }

            List<(BucketId, IoHashKey, DateTime)> records = await ReferencesStore.GetRecords(TestNamespace).ToListAsync();

            (BucketId, IoHashKey, DateTime)? oldRecord = records.Find(record => record.Item2 == key);
            Assert.IsNotNull(oldRecord);
            Assert.AreEqual(key, oldRecord.Value.Item2);
            Assert.AreEqual("bucket", oldRecord.Value.Item1.ToString());
        }
        
        [TestMethod]
        public async Task BatchJsonRequest()
        {
            // verifies that json request against the batch endpoint will fail
            {
                using HttpContent requestContent = new StringContent("{}");
                requestContent.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Json);

                HttpResponseMessage result = await Client!.PostAsync(new Uri($"api/v1/refs/{TestNamespace}", UriKind.Relative), requestContent);

                Assert.AreEqual(HttpStatusCode.UnsupportedMediaType, result.StatusCode);
            }
        }

        [TestMethod]
        public async Task BatchErrorOperations()
        {
            // seed some data
            BucketId bucket = new BucketId("bucket");
            IoHashKey newBlobObjectKey = IoHashKey.FromName("thisObjectDoesNotExist");

            IoHashKey putObjectKey = IoHashKey.FromName("putObjectFail");
            CbObject ref1 = CbObject.Empty;

            CbWriter getObjectOp = new CbWriter();
            getObjectOp.BeginObject();
            getObjectOp.WriteInteger( "opId",0);
            getObjectOp.WriteString("op", BatchOps.BatchOp.Operation.GET.ToString());
            getObjectOp.WriteString("bucket", bucket.ToString());
            getObjectOp.WriteString("key", newBlobObjectKey.ToString());
            getObjectOp.EndObject();

            CbWriter putObjectOp = new CbWriter();
            putObjectOp.BeginObject();
            putObjectOp.WriteInteger("opId", 1);
            putObjectOp.WriteString("op", BatchOps.BatchOp.Operation.PUT.ToString());
            putObjectOp.WriteString("bucket", bucket.ToString());
            putObjectOp.WriteString("key", putObjectKey.ToString());
            putObjectOp.WriteObject("payload", ref1);
            // omit the hash for a error object1.WriteHash("payloadHash", IoHash.Compute(ref1.GetView().Span));
            putObjectOp.EndObject();

            CbObject[] ops = new[]
            {
                getObjectOp.ToObject(),
                putObjectOp.ToObject(),
            };

            CbWriter batchRequestWriter = new CbWriter();
            batchRequestWriter.BeginObject();
            batchRequestWriter.BeginUniformArray("ops", CbFieldType.Object);
            foreach (CbObject o in ops)
            {
                batchRequestWriter.WriteObject(o);
            }
            batchRequestWriter.EndUniformArray();
            batchRequestWriter.EndObject();
            byte[] batchRequestData = batchRequestWriter.ToByteArray();

            {
                using HttpContent requestContent = new ByteArrayContent(batchRequestData);
                requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);

                HttpResponseMessage result = await Client!.PostAsync(new Uri($"api/v1/refs/{TestNamespace}", UriKind.Relative), requestContent);
                result.EnsureSuccessStatusCode();

                await using MemoryStream ms = new MemoryStream();
                await result.Content.CopyToAsync(ms);
                byte[] roundTrippedBuffer = ms.ToArray();

                BatchOpsResponse response = CbSerializer.Deserialize<BatchOpsResponse>(roundTrippedBuffer);
                Assert.AreEqual(2, response.Results.Count);
                
                BatchOpsResponse.OpResponses op0 = response.Results.First(r => r.OpId == 0);
                Assert.IsNotNull(op0.Response);
                Assert.AreEqual(404, op0.StatusCode);
                Assert.IsTrue(!op0.Response["title"].Equals(CbField.Empty));
                Assert.AreEqual("Object not found 29911b58b3c970ba39d9690f2dca66839dd6f5d9 in bucket bucket namespace test-namespace", op0.Response["title"].AsString());

                BatchOpsResponse.OpResponses op1 = response.Results.First(r => r.OpId == 1);
                Assert.IsNotNull(op1.Response);
                Assert.AreEqual(500, op1.StatusCode);
                Assert.IsTrue(!op1.Response["title"].Equals(CbField.Empty));
                Assert.AreEqual("Missing payload for operation: 1", op1.Response["title"].AsString());
            }
        }

        [TestMethod]
        public async Task BatchGetOperations()
        {
            // seed some data
            BucketId bucket = new BucketId("bucket");
            IoHashKey newBlobObjectKey = IoHashKey.FromName("newBlobObject");
            CbObject newBlobObject = CbObject.Build(writer => writer.WriteString("String", "this-has-contents"));

            {
                byte[] cbObjectBytes = newBlobObject.GetView().ToArray();
                BlobIdentifier blobHash = BlobIdentifier.FromBlob(cbObjectBytes);

                using HttpContent requestContent = new ByteArrayContent(cbObjectBytes);
                requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);
                requestContent.Headers.Add(CommonHeaders.HashHeaderName, blobHash.ToString());

                HttpResponseMessage result = await Client!.PutAsync(new Uri($"api/v1/refs/{TestNamespace}/{bucket}/{newBlobObjectKey}.uecb", UriKind.Relative), requestContent);
                result.EnsureSuccessStatusCode();
                byte[] content = await result.Content.ReadAsByteArrayAsync();
                PutObjectResponse? response = CbSerializer.Deserialize<PutObjectResponse?>(content);
                Assert.IsNotNull(response);
                Assert.IsTrue(response.Needs.Length == 0);
            }

            byte[] blobContents = Encoding.ASCII.GetBytes("This is a attached blob");
            CbObject newReferenceObject = CbObject.Build(writer => writer.WriteBinaryAttachment("Attachment", IoHash.Compute(blobContents)));
            IoHashKey newReferenceObjectKey = IoHashKey.FromName("newReferenceObject");

            {
                BlobIdentifier blobHash = BlobIdentifier.FromBlob(blobContents);

                using HttpContent requestContent = new ByteArrayContent(blobContents);
                requestContent.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Octet);
                requestContent.Headers.Add(CommonHeaders.HashHeaderName, blobHash.ToString());

                HttpResponseMessage result = await Client!.PutAsync(new Uri($"api/v1/blobs/{TestNamespace}/{blobHash}", UriKind.Relative), requestContent);
                result.EnsureSuccessStatusCode();
            }

            byte[] blobContentsMissing = Encoding.ASCII.GetBytes("This contents will not be submitted");
            CbObject missingAttachmentObject = CbObject.Build(writer => writer.WriteBinaryAttachment("Attachment", IoHash.Compute(blobContentsMissing)));
            IoHashKey missingAttachmentKey = IoHashKey.FromName("blobMissingAttachment");

            {
                BlobIdentifier blobHash = BlobIdentifier.FromBlob(blobContents);

                using HttpContent requestContent = new ByteArrayContent(blobContents);
                requestContent.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Octet);
                requestContent.Headers.Add(CommonHeaders.HashHeaderName, blobHash.ToString());

                HttpResponseMessage result = await Client!.PutAsync(new Uri($"api/v1/blobs/{TestNamespace}/{blobHash}", UriKind.Relative), requestContent);
                result.EnsureSuccessStatusCode();
            }

            {
                byte[] cbObjectBytes = newReferenceObject.GetView().ToArray();
                BlobIdentifier blobHash = BlobIdentifier.FromBlob(cbObjectBytes);

                using HttpContent requestContent = new ByteArrayContent(cbObjectBytes);
                requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);
                requestContent.Headers.Add(CommonHeaders.HashHeaderName, blobHash.ToString());

                HttpResponseMessage result = await Client!.PutAsync(new Uri($"api/v1/refs/{TestNamespace}/{bucket}/{newReferenceObjectKey}.uecb", UriKind.Relative), requestContent);
                result.EnsureSuccessStatusCode();

                result.EnsureSuccessStatusCode();
                byte[] content = await result.Content.ReadAsByteArrayAsync();
                PutObjectResponse response = CbSerializer.Deserialize<PutObjectResponse>(content);
                Assert.IsTrue(response.Needs.Length == 0);
            }
            CbWriter getObjectOp = new CbWriter();
            getObjectOp.BeginObject();
            getObjectOp.WriteInteger( "opId",0);
            getObjectOp.WriteString("op", BatchOps.BatchOp.Operation.GET.ToString());
            getObjectOp.WriteString("bucket", bucket.ToString());
            getObjectOp.WriteString("key", newBlobObjectKey.ToString());
            getObjectOp.EndObject();

            CbWriter getObjectOp2 = new CbWriter();
            getObjectOp2.BeginObject();
            getObjectOp2.WriteInteger("opId", 1);
            getObjectOp2.WriteString("op", BatchOps.BatchOp.Operation.GET.ToString());
            getObjectOp2.WriteString("bucket", bucket.ToString());
            getObjectOp2.WriteString("key", newReferenceObjectKey.ToString());
            getObjectOp2.EndObject();

            
            CbWriter getObjectOp3 = new CbWriter();
            getObjectOp3.BeginObject();
            getObjectOp3.WriteInteger("opId", 2);
            getObjectOp3.WriteString("op", BatchOps.BatchOp.Operation.GET.ToString());
            getObjectOp3.WriteString("bucket", bucket.ToString());
            getObjectOp3.WriteString("key", missingAttachmentKey.ToString());
            getObjectOp3.WriteBool("resolveAttachments", true);
            getObjectOp3.EndObject();

            CbObject[] ops = new[]
            {
                getObjectOp.ToObject(),
                getObjectOp2.ToObject(),
                getObjectOp3.ToObject(),
            };

            CbWriter batchRequestWriter = new CbWriter();
            batchRequestWriter.BeginObject();
            batchRequestWriter.BeginUniformArray("ops", CbFieldType.Object);
            foreach (CbObject o in ops)
            {
                batchRequestWriter.WriteObject(o);
            }
            batchRequestWriter.EndUniformArray();
            batchRequestWriter.EndObject();
            byte[] batchRequestData = batchRequestWriter.ToByteArray();

            {
                using HttpContent requestContent = new ByteArrayContent(batchRequestData);
                requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);

                HttpResponseMessage result = await Client!.PostAsync(new Uri($"api/v1/refs/{TestNamespace}", UriKind.Relative), requestContent);
                result.EnsureSuccessStatusCode();

                await using MemoryStream ms = new MemoryStream();
                await result.Content.CopyToAsync(ms);
                byte[] roundTrippedBuffer = ms.ToArray();

                BatchOpsResponse response = CbSerializer.Deserialize<BatchOpsResponse>(roundTrippedBuffer);
                Assert.AreEqual(3, response.Results.Count);
                
                BatchOpsResponse.OpResponses op0 = response.Results.First(r => r.OpId == 0);
                Assert.IsNotNull(op0.Response);
                Assert.AreEqual(200, op0.StatusCode);
                Assert.AreEqual(newBlobObject, op0.Response);

                BatchOpsResponse.OpResponses op1 = response.Results.First(r => r.OpId == 1);
                Assert.IsNotNull(op1.Response);
                Assert.AreEqual(200, op1.StatusCode);
                Assert.AreEqual(newReferenceObject, op1.Response);

                BatchOpsResponse.OpResponses op2 = response.Results.First(r => r.OpId == 2);
                Assert.IsNotNull(op2.Response);
                Assert.AreEqual(404, op2.StatusCode);
                Assert.AreNotEqual(missingAttachmentObject, op2.Response);
            }
        }

        [TestMethod]
        public async Task BatchHeadOperations()
        {
            // seed some data
            BucketId bucket = new BucketId("bucket");
            IoHashKey newBlobObjectKey = IoHashKey.FromName("newBlobObject");
            CbObject newBlobObject = CbObject.Build(writer => writer.WriteString("String", "this-has-contents"));

            {
                byte[] cbObjectBytes = newBlobObject.GetView().ToArray();
                BlobIdentifier blobHash = BlobIdentifier.FromBlob(cbObjectBytes);

                using HttpContent requestContent = new ByteArrayContent(cbObjectBytes);
                requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);
                requestContent.Headers.Add(CommonHeaders.HashHeaderName, blobHash.ToString());

                HttpResponseMessage result = await Client!.PutAsync(new Uri($"api/v1/refs/{TestNamespace}/{bucket}/{newBlobObjectKey}.uecb", UriKind.Relative), requestContent);
                result.EnsureSuccessStatusCode();
                byte[] content = await result.Content.ReadAsByteArrayAsync();
                PutObjectResponse? response = CbSerializer.Deserialize<PutObjectResponse?>(content);
                Assert.IsNotNull(response);
                Assert.IsTrue(response.Needs.Length == 0);
            }

            byte[] blobContents = Encoding.ASCII.GetBytes("This is a attached blob");
            CbObject newReferenceObject = CbObject.Build(writer => writer.WriteBinaryAttachment("Attachment", IoHash.Compute(blobContents)));
            IoHashKey newReferenceObjectKey = IoHashKey.FromName("newReferenceObject");

            {
                BlobIdentifier blobHash = BlobIdentifier.FromBlob(blobContents);

                using HttpContent requestContent = new ByteArrayContent(blobContents);
                requestContent.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Octet);
                requestContent.Headers.Add(CommonHeaders.HashHeaderName, blobHash.ToString());

                HttpResponseMessage result = await Client!.PutAsync(new Uri($"api/v1/blobs/{TestNamespace}/{blobHash}", UriKind.Relative), requestContent);
                result.EnsureSuccessStatusCode();
            }

            {
                byte[] cbObjectBytes = newReferenceObject.GetView().ToArray();
                BlobIdentifier blobHash = BlobIdentifier.FromBlob(cbObjectBytes);

                using HttpContent requestContent = new ByteArrayContent(cbObjectBytes);
                requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);
                requestContent.Headers.Add(CommonHeaders.HashHeaderName, blobHash.ToString());

                HttpResponseMessage result = await Client!.PutAsync(new Uri($"api/v1/refs/{TestNamespace}/{bucket}/{newReferenceObjectKey}.uecb", UriKind.Relative), requestContent);
                result.EnsureSuccessStatusCode();

                result.EnsureSuccessStatusCode();
                byte[] content = await result.Content.ReadAsByteArrayAsync();
                PutObjectResponse response = CbSerializer.Deserialize<PutObjectResponse>(content);
                Assert.IsTrue(response.Needs.Length == 0);
            }
            CbWriter getObjectOp = new CbWriter();
            getObjectOp.BeginObject();
            getObjectOp.WriteInteger( "opId",0);
            getObjectOp.WriteString("op", BatchOps.BatchOp.Operation.HEAD.ToString());
            getObjectOp.WriteString("bucket", bucket.ToString());
            getObjectOp.WriteString("key", newBlobObjectKey.ToString());
            getObjectOp.EndObject();

            CbWriter getObjectOp2 = new CbWriter();
            getObjectOp2.BeginObject();
            getObjectOp2.WriteInteger("opId", 1);
            getObjectOp2.WriteString("op", BatchOps.BatchOp.Operation.HEAD.ToString());
            getObjectOp2.WriteString("bucket", bucket.ToString());
            getObjectOp2.WriteString("key", newReferenceObjectKey.ToString());
            getObjectOp2.EndObject();

            CbObject[] ops = new[]
            {
                getObjectOp.ToObject(),
                getObjectOp2.ToObject(),
            };

            CbWriter batchRequestWriter = new CbWriter();
            batchRequestWriter.BeginObject();
            batchRequestWriter.BeginUniformArray("ops", CbFieldType.Object);
            foreach (CbObject o in ops)
            {
                batchRequestWriter.WriteObject(o);
            }
            batchRequestWriter.EndUniformArray();
            batchRequestWriter.EndObject();
            byte[] batchRequestData = batchRequestWriter.ToByteArray();
            
            {
                using HttpContent requestContent = new ByteArrayContent(batchRequestData);
                requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);

                HttpResponseMessage result = await Client!.PostAsync(new Uri($"api/v1/refs/{TestNamespace}", UriKind.Relative), requestContent);
                result.EnsureSuccessStatusCode();

                await using MemoryStream ms = new MemoryStream();
                await result.Content.CopyToAsync(ms);
                byte[] roundTrippedBuffer = ms.ToArray();

                BatchOpsResponse response = CbSerializer.Deserialize<BatchOpsResponse>(roundTrippedBuffer);
                Assert.AreEqual(2, response.Results.Count);
                
                BatchOpsResponse.OpResponses op0 = response.Results.First(r => r.OpId == 0);
                Assert.IsNotNull(op0.Response);
                Assert.AreEqual(200, op0.StatusCode);
                Assert.IsTrue(!op0.Response["exists"].Equals(CbField.Empty));
                Assert.IsTrue(op0.Response["exists"].AsBool());

                BatchOpsResponse.OpResponses op1 = response.Results.First(r => r.OpId == 1);
                Assert.IsNotNull(op1.Response);
                Assert.AreEqual(200, op1.StatusCode);
                Assert.IsTrue(!op1.Response["exists"].Equals(CbField.Empty));
                Assert.IsTrue(op1.Response["exists"].AsBool());
            }
        }

        [TestMethod]
        public async Task BatchPutOperations()
        {
            BucketId bucket = new BucketId("bucket");
            IoHashKey ref0name = IoHashKey.FromName("putRef0");
            CbObject ref0 = CbObject.Build(writer => writer.WriteString("foo", "bar"));

            IoHashKey ref1name = IoHashKey.FromName("putRef1");
            CbObject ref1 = CbObject.Build(writer => writer.WriteInteger("baz", 1337));

            CbWriter object0 = new CbWriter();
            object0.BeginObject();
            object0.WriteInteger( "opId",0);
            object0.WriteString("op", BatchOps.BatchOp.Operation.PUT.ToString());
            object0.WriteString("bucket", bucket.ToString());
            object0.WriteString("key", ref0name.ToString());
            object0.WriteObject("payload", ref0);
            object0.WriteHash("payloadHash", IoHash.Compute(ref0.GetView().Span));
            object0.EndObject();

            CbWriter object1 = new CbWriter();
            object1.BeginObject();
            object1.WriteInteger("opId", 1);
            object1.WriteString("op", BatchOps.BatchOp.Operation.PUT.ToString());
            object1.WriteString("bucket", bucket.ToString());
            object1.WriteString("key", ref1name.ToString());
            object1.WriteObject("payload", ref1);
            object1.WriteHash("payloadHash", IoHash.Compute(ref1.GetView().Span));
            object1.EndObject();

            CbObject[] ops = new[]
            {
                object0.ToObject(),
                object1.ToObject(),
            };

            CbWriter batchRequestWriter = new CbWriter();
            batchRequestWriter.BeginObject();
            batchRequestWriter.BeginUniformArray("ops", CbFieldType.Object);
            foreach (CbObject o in ops)
            {
                batchRequestWriter.WriteObject(o);
            }
            batchRequestWriter.EndUniformArray();
            batchRequestWriter.EndObject();
            byte[] batchRequestData = batchRequestWriter.ToByteArray();

            {
                using HttpContent requestContent = new ByteArrayContent(batchRequestData);
                requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);

                HttpResponseMessage result = await Client!.PostAsync(new Uri($"api/v1/refs/{TestNamespace}", UriKind.Relative), requestContent);
                result.EnsureSuccessStatusCode();

                await using MemoryStream ms = new MemoryStream();
                await result.Content.CopyToAsync(ms);
                byte[] roundTrippedBuffer = ms.ToArray();

                BatchOpsResponse response = CbSerializer.Deserialize<BatchOpsResponse>(roundTrippedBuffer);
                Assert.AreEqual(2, response.Results.Count);
                
                BatchOpsResponse.OpResponses op0 = response.Results.First(r => r.OpId == 0);
                Assert.IsNotNull(op0.Response);
                Assert.AreEqual(200, op0.StatusCode, $"Expected 200 response, got {op0.StatusCode} . Response: {op0.Response.ToJson()}");
                Assert.IsTrue(!op0.Response["needs"].Equals(CbField.Empty));
                CollectionAssert.AreEqual(Array.Empty<IoHash>(), op0.Response["needs"].ToArray());

                BatchOpsResponse.OpResponses op1 = response.Results.First(r => r.OpId == 1);
                Assert.IsNotNull(op1.Response);
                Assert.AreEqual(200, op1.StatusCode, $"Expected 200 response, got {op1.StatusCode} . Response: {op1.Response.ToJson()}");
                Assert.IsTrue(!op1.Response["needs"].Equals(CbField.Empty));
                CollectionAssert.AreEqual(Array.Empty<IoHash>(), op1.Response["needs"].ToArray());
            }
        }

        [TestMethod]
        public async Task BatchMixedOperations()
        {
            // seed some data
            BucketId bucket = new BucketId("bucket");
            IoHashKey getObjectKey = IoHashKey.FromName("getBlobObject");
            CbObject getObject = CbObject.Build(writer => writer.WriteString("String", "this-has-contents"));

            IoHashKey putObjectKey = IoHashKey.FromName("putBlobObject");
            CbObject putObject = CbObject.Build(writer => writer.WriteString("String", "this-has-contents"));

            IoHashKey missingObjectKey = IoHashKey.FromName("thisKeyDoesNotExist");

            {
                byte[] cbObjectBytes = getObject.GetView().ToArray();
                BlobIdentifier blobHash = BlobIdentifier.FromBlob(cbObjectBytes);

                using HttpContent requestContent = new ByteArrayContent(cbObjectBytes);
                requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);
                requestContent.Headers.Add(CommonHeaders.HashHeaderName, blobHash.ToString());

                HttpResponseMessage result = await Client!.PutAsync(new Uri($"api/v1/refs/{TestNamespace}/{bucket}/{getObjectKey}.uecb", UriKind.Relative), requestContent);
                result.EnsureSuccessStatusCode();
                byte[] content = await result.Content.ReadAsByteArrayAsync();
                PutObjectResponse? response = CbSerializer.Deserialize<PutObjectResponse?>(content);
                Assert.IsNotNull(response);
                Assert.IsTrue(response.Needs.Length == 0);
            }

            CbWriter getObjectOp = new CbWriter();
            getObjectOp.BeginObject();
            getObjectOp.WriteInteger( "opId",0);
            getObjectOp.WriteString("op", BatchOps.BatchOp.Operation.GET.ToString());
            getObjectOp.WriteString("bucket", bucket.ToString());
            getObjectOp.WriteString("key", getObjectKey.ToString());
            getObjectOp.EndObject();

            CbWriter putObjectOp = new CbWriter();
            putObjectOp.BeginObject();
            putObjectOp.WriteInteger("opId", 1);
            putObjectOp.WriteString("op", BatchOps.BatchOp.Operation.PUT.ToString());
            putObjectOp.WriteString("bucket", bucket.ToString());
            putObjectOp.WriteString("key", putObjectKey.ToString());
            putObjectOp.WriteObject("payload", putObject);
            putObjectOp.WriteHash("payloadHash", IoHash.Compute(putObject.GetView().Span));
            putObjectOp.EndObject();

            CbWriter errorObjectOp = new CbWriter();
            errorObjectOp.BeginObject();
            errorObjectOp.WriteInteger( "opId",2);
            errorObjectOp.WriteString("op", BatchOps.BatchOp.Operation.GET.ToString());
            errorObjectOp.WriteString("bucket", bucket.ToString());
            errorObjectOp.WriteString("key", missingObjectKey.ToString());
            errorObjectOp.EndObject();

            CbWriter headObjectOp = new CbWriter();
            headObjectOp.BeginObject();
            headObjectOp.WriteInteger("opId", 3);
            headObjectOp.WriteString("op", BatchOps.BatchOp.Operation.HEAD.ToString());
            headObjectOp.WriteString("bucket", bucket.ToString());
            headObjectOp.WriteString("key", getObjectKey.ToString());
            headObjectOp.EndObject();

            CbObject[] ops = new[]
            {
                getObjectOp.ToObject(),
                putObjectOp.ToObject(),
                errorObjectOp.ToObject(),
                headObjectOp.ToObject(),
            };

            CbWriter batchRequestWriter = new CbWriter();
            batchRequestWriter.BeginObject();
            batchRequestWriter.BeginUniformArray("ops", CbFieldType.Object);
            foreach (CbObject o in ops)
            {
                batchRequestWriter.WriteObject(o);
            }
            batchRequestWriter.EndUniformArray();
            batchRequestWriter.EndObject();
            byte[] batchRequestData = batchRequestWriter.ToByteArray();
            
            {
                using HttpContent requestContent = new ByteArrayContent(batchRequestData);
                requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);

                HttpResponseMessage result = await Client!.PostAsync(new Uri($"api/v1/refs/{TestNamespace}", UriKind.Relative), requestContent);
                result.EnsureSuccessStatusCode();

                await using MemoryStream ms = new MemoryStream();
                await result.Content.CopyToAsync(ms);
                byte[] roundTrippedBuffer = ms.ToArray();

                BatchOpsResponse response = CbSerializer.Deserialize<BatchOpsResponse>(roundTrippedBuffer);
                Assert.AreEqual(4, response.Results.Count);
                
                BatchOpsResponse.OpResponses getOp = response.Results.First(r => r.OpId == 0);
                Assert.IsNotNull(getOp.Response);
                Assert.AreEqual(200, getOp.StatusCode);
                Assert.AreEqual(getObject, getOp.Response);

                BatchOpsResponse.OpResponses putOp = response.Results.First(r => r.OpId == 1);
                Assert.IsNotNull(putOp.Response);
                Assert.AreEqual(200, putOp.StatusCode);
                Assert.IsTrue(!putOp.Response["needs"].Equals(CbField.Empty));
                CollectionAssert.AreEqual(Array.Empty<IoHash>(), putOp.Response["needs"].ToArray());

                BatchOpsResponse.OpResponses errorOp = response.Results.First(r => r.OpId == 2);
                Assert.IsNotNull(errorOp.Response);
                Assert.AreEqual(404, errorOp.StatusCode);
                Assert.IsTrue(!errorOp.Response["title"].Equals(CbField.Empty));
                Assert.AreEqual("Object not found cbc3db15b9c8253f6106158962325c8fd848daef in bucket bucket namespace test-namespace", errorOp.Response["title"].AsString());
                Assert.AreEqual(404, errorOp.Response["status"].AsInt32());

                BatchOpsResponse.OpResponses headOp = response.Results.First(r => r.OpId == 3);
                Assert.IsNotNull(headOp.Response);
                Assert.AreEqual(200, headOp.StatusCode);
                Assert.IsTrue(!headOp.Response["exists"].Equals(CbField.Empty));
                Assert.IsTrue(headOp.Response["exists"].AsBool());
            }
        }

        [TestMethod]
        public async Task GetPackage()
        {
            string blobContents = "This is a string that is referenced as a blob";
            byte[] blobData = Encoding.ASCII.GetBytes(blobContents);
            BlobIdentifier blobHash = BlobIdentifier.FromBlob(blobData);
            await Service.PutObject(TestNamespace, blobData, blobHash);

            string blobContentsChild = "This string is also referenced as a blob but from a child object";
            byte[] dataChild = Encoding.ASCII.GetBytes(blobContentsChild);
            BlobIdentifier blobHashChild = BlobIdentifier.FromBlob(dataChild);
            await Service.PutObject(TestNamespace, dataChild, blobHashChild);

            CbWriter writerChild = new CbWriter();
            writerChild.BeginObject();
            writerChild.WriteBinaryAttachment("blob", blobHashChild.AsIoHash());
            writerChild.EndObject();

            byte[] childDataObject = writerChild.ToByteArray();
            BlobIdentifier childDataObjectHash = BlobIdentifier.FromBlob(childDataObject);
            await Service.PutObject(TestNamespace, childDataObject, childDataObjectHash);

            CbWriter writerParent = new CbWriter();
            writerParent.BeginObject();

            writerParent.WriteBinaryAttachment("blobAttachment", blobHash.AsIoHash());
            writerParent.WriteObjectAttachment("objectAttachment", childDataObjectHash.AsIoHash());
            writerParent.EndObject();

            byte[] objectData = writerParent.ToByteArray();
            BlobIdentifier objectHash = BlobIdentifier.FromBlob(objectData);

            IoHashKey key = IoHashKey.FromName("newHierarchyObject");

            using HttpContent requestContent = new ByteArrayContent(objectData);
            requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);
            requestContent.Headers.Add(CommonHeaders.HashHeaderName, objectHash.ToString());

            {
                HttpResponseMessage result = await Client!.PutAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}.uecb", UriKind.Relative), requestContent);
                result.EnsureSuccessStatusCode();
            }

            {
                HttpResponseMessage result = await Client!.GetAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}.uecbpkg", UriKind.Relative));
                result.EnsureSuccessStatusCode();

                CbPackageReader packageReader = await CbPackageReader.Create(await result.Content.ReadAsStreamAsync());
                List<(CbPackageAttachmentEntry, byte[])>? attachments = await packageReader.IterateAttachments().ToListAsync();

                Assert.AreEqual(3, attachments.Count);

                foreach ((CbPackageAttachmentEntry entry, byte[] blob) in attachments)
                {
                    if (entry.AttachmentHash == blobHash.AsIoHash())
                    {
                        CollectionAssert.AreEqual(blobData, blob);
                    }
                    else if (entry.AttachmentHash == blobHashChild.AsIoHash())
                    {
                        CollectionAssert.AreEqual(dataChild,  blob);
                    }
                    else if (entry.AttachmentHash == childDataObjectHash.AsIoHash())
                    {
                        CollectionAssert.AreEqual(childDataObject, blob);
                    }
                    else
                    {
                        Assert.Fail($"Unknown attachment {entry.AttachmentHash}");
                    }
                }
            }
        }

        [TestMethod]
        public async Task PutPackage()
        {
            string blobContents = "This is a string that is referenced as a blob";
            byte[] blobData = Encoding.ASCII.GetBytes(blobContents);
            BlobIdentifier blobHash = BlobIdentifier.FromBlob(blobData);
            await Service.PutObject(TestNamespace, blobData, blobHash);

            string blobContentsChild = "This string is also referenced as a blob but from a child object";
            byte[] dataChild = Encoding.ASCII.GetBytes(blobContentsChild);
            BlobIdentifier blobHashChild = BlobIdentifier.FromBlob(dataChild);
            await Service.PutObject(TestNamespace, dataChild, blobHashChild);

            CbWriter writerChild = new CbWriter();
            writerChild.BeginObject();
            writerChild.WriteBinaryAttachment("blob", blobHashChild.AsIoHash());
            writerChild.EndObject();

            byte[] childDataObject = writerChild.ToByteArray();
            BlobIdentifier childDataObjectHash = BlobIdentifier.FromBlob(childDataObject);
            await Service.PutObject(TestNamespace, childDataObject, childDataObjectHash);

            CbWriter writerParent = new CbWriter();
            writerParent.BeginObject();

            writerParent.WriteBinaryAttachment("blobAttachment", blobHash.AsIoHash());
            writerParent.WriteObjectAttachment("objectAttachment", childDataObjectHash.AsIoHash());
            writerParent.EndObject();

            byte[] objectData = writerParent.ToByteArray();
            BlobIdentifier objectHash = BlobIdentifier.FromBlob(objectData);

            IoHashKey key = IoHashKey.FromName("newHierarchyObject");

            CbPackageBuilder builder = new CbPackageBuilder();
            await builder.AddAttachment(objectHash.AsIoHash(), CbPackageAttachmentFlags.IsObject, objectData);
            await builder.AddAttachment(blobHash.AsIoHash(), 0, blobData);
            await builder.AddAttachment(blobHashChild.AsIoHash(), 0, dataChild);
            await builder.AddAttachment(childDataObjectHash.AsIoHash(), 0, childDataObject);
            using HttpContent requestContent = new ByteArrayContent(builder.ToByteArray());
            requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinaryPackage);

            {
                HttpResponseMessage result = await Client!.PutAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}", UriKind.Relative), requestContent);
                result.EnsureSuccessStatusCode();
            }
            
            {
                BlobIdentifier blobAttachment;
                BlobIdentifier objectAttachment;
                {
                    HttpResponseMessage getResponse = await Client.GetAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}.uecb", UriKind.Relative));
                    getResponse.EnsureSuccessStatusCode();
                    await using MemoryStream ms = new MemoryStream();
                    await getResponse.Content.CopyToAsync(ms);

                    byte[] roundTrippedBuffer = ms.ToArray();
                    ReadOnlyMemory<byte> localMemory = new ReadOnlyMemory<byte>(roundTrippedBuffer);
                    CbObject cb = new CbObject(roundTrippedBuffer);
                    Assert.AreEqual(2, cb.Count());

                    CbField blobAttachmentField = cb["blobAttachment"];
                    Assert.AreNotEqual(CbField.Empty, blobAttachmentField);
                    blobAttachment = BlobIdentifier.FromIoHash(blobAttachmentField.AsBinaryAttachment());
                    CbField objectAttachmentField = cb["objectAttachment"];
                    Assert.AreNotEqual(CbField.Empty, objectAttachmentField);
                    objectAttachment = BlobIdentifier.FromIoHash(objectAttachmentField.AsObjectAttachment().Hash);
                }

                {
                    HttpResponseMessage getAttachment = await Client.GetAsync(new Uri($"api/v1/blobs/{TestNamespace}/{blobAttachment}", UriKind.Relative));
                    getAttachment.EnsureSuccessStatusCode();
                    await using MemoryStream ms = new MemoryStream();
                    await getAttachment.Content.CopyToAsync(ms);
                    byte[] roundTrippedBuffer = ms.ToArray();
                    string roundTrippedString = Encoding.ASCII.GetString(roundTrippedBuffer);

                    Assert.AreEqual(blobContents, roundTrippedString);
                    Assert.AreEqual(blobHash, BlobIdentifier.FromBlob(roundTrippedBuffer));
                }

                BlobIdentifier attachedBlobIdentifier;
                {
                    HttpResponseMessage getAttachment = await Client.GetAsync(new Uri($"api/v1/blobs/{TestNamespace}/{objectAttachment}", UriKind.Relative));
                    getAttachment.EnsureSuccessStatusCode();
                    await using MemoryStream ms = new MemoryStream();
                    await getAttachment.Content.CopyToAsync(ms);
                    byte[] roundTrippedBuffer = ms.ToArray();
                    ReadOnlyMemory<byte> localMemory = new ReadOnlyMemory<byte>(roundTrippedBuffer);
                    CbObject cb = new CbObject(roundTrippedBuffer);
                    Assert.AreEqual(1, cb.Count());

                    CbField blobField = cb["blob"];
                    Assert.AreNotEqual(CbField.Empty, blobField);

                    attachedBlobIdentifier = BlobIdentifier.FromIoHash(blobField!.AsBinaryAttachment());
                }

                {
                    HttpResponseMessage getAttachment = await Client.GetAsync(new Uri($"api/v1/blobs/{TestNamespace}/{attachedBlobIdentifier}", UriKind.Relative));
                    getAttachment.EnsureSuccessStatusCode();
                    await using MemoryStream ms = new MemoryStream();
                    await getAttachment.Content.CopyToAsync(ms);
                    byte[] roundTrippedBuffer = ms.ToArray();
                    string roundTrippedString = Encoding.ASCII.GetString(roundTrippedBuffer);

                    Assert.AreEqual(blobContentsChild, roundTrippedString);
                    Assert.AreEqual(blobHashChild, BlobIdentifier.FromBlob(roundTrippedBuffer));
                }
            }
        }

        [TestMethod]
        public async Task PutPackageWithError()
        {
            string blobContents = "This is a string that is referenced as a blob";
            byte[] blobData = Encoding.ASCII.GetBytes(blobContents);
            BlobIdentifier blobHash = BlobIdentifier.FromBlob(blobData);
            await Service.PutObject(TestNamespace, blobData, blobHash);

            CbWriter writerParent = new CbWriter();
            writerParent.BeginObject();
            writerParent.WriteBinaryAttachment("blobAttachment", blobHash.AsIoHash());
            writerParent.EndObject();

            byte[] objectData = writerParent.ToByteArray();
            BlobIdentifier objectHash = BlobIdentifier.FromBlob(objectData);

            IoHashKey key = IoHashKey.FromName("brokenObject");

            CbPackageBuilder builder = new CbPackageBuilder();
            await builder.AddAttachment(objectHash.AsIoHash(), CbPackageAttachmentFlags.IsObject, objectData);
            await builder.AddAttachment(blobHash.AsIoHash(), CbPackageAttachmentFlags.IsError, blobData);
            using HttpContent requestContent = new ByteArrayContent(builder.ToByteArray());
            requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinaryPackage);

            {
                HttpResponseMessage result = await Client!.PutAsync(new Uri($"api/v1/refs/{TestNamespace}/bucket/{key}", UriKind.Relative), requestContent);
                Assert.AreEqual(HttpStatusCode.BadRequest, result.StatusCode);
            }
        }
    }
}
