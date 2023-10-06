// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Net.Http.Json;
using System.Text;
using System.Threading.Tasks;
using Amazon.S3;
using Amazon.S3.Model;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Backends;
using Jupiter.Controllers;
using Jupiter.Implementation;
using Jupiter.Implementation.Blob;
using Microsoft.AspNetCore.TestHost;
using Microsoft.AspNetCore.Hosting;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging.Abstractions;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Serilog;
using Serilog.Core;

namespace Jupiter.FunctionalTests.Storage;

[TestClass]
public class MemoryBundlesTests : BundlesTests
{
    private IAmazonS3? _s3;
    protected override IEnumerable<KeyValuePair<string, string>> GetSettings()
    {
        return new[]
        {
            new KeyValuePair<string, string>("UnrealCloudDDC:StorageImplementations:0", UnrealCloudDDCSettings.StorageBackendImplementations.S3.ToString()),
            new KeyValuePair<string, string>("S3:BucketName", $"tests-{TestNamespaceName}"),
            new KeyValuePair<string, string>("UnrealCloudDDC:BlobIndexImplementation", "Memory"),
            new KeyValuePair<string, string>("UnrealCloudDDC:ReferencesDbImplementation", "Memory"),
        };
    }

    protected override async Task Seed(IServiceProvider provider)
    {
        string s3BucketName = $"tests-{TestNamespaceName}";

        _s3 = provider.GetService<IAmazonS3>();
        Assert.IsNotNull(_s3);
        if (await _s3!.DoesS3BucketExistAsync(s3BucketName))
        {
            // if we have failed to run the cleanup for some reason we run it now
            await Teardown(provider);
        }

        await _s3.PutBucketAsync(s3BucketName);
        await _s3.PutObjectAsync(new PutObjectRequest { BucketName = s3BucketName, Key = BlobIdentifier.FromBlobLocator(SmallFileLocator).AsS3Key(), ContentBody = SmallFileContents });
    }

    protected override async Task Teardown(IServiceProvider provider)
    {
        string s3BucketName = $"tests-{TestNamespaceName}";
        ListObjectsResponse response = await _s3!.ListObjectsAsync(s3BucketName);
        List<KeyVersion> objectKeys = response.S3Objects.Select(o => new KeyVersion { Key = o.Key }).ToList();
        await _s3.DeleteObjectsAsync(new DeleteObjectsRequest { BucketName = s3BucketName, Objects = objectKeys });

        await _s3.DeleteBucketAsync(s3BucketName);
    }
}

[TestClass]
public class ScyllaBundlesTests : BundlesTests
{
    private IAmazonS3? _s3;
    protected override IEnumerable<KeyValuePair<string, string>> GetSettings()
    {
        return new[]
        {
            new KeyValuePair<string, string>("UnrealCloudDDC:StorageImplementations:0", UnrealCloudDDCSettings.StorageBackendImplementations.S3.ToString()),
            new KeyValuePair<string, string>("S3:BucketName", $"tests-{TestNamespaceName}"),
            new KeyValuePair<string, string>("UnrealCloudDDC:BlobIndexImplementation", "Scylla"),
            new KeyValuePair<string, string>("UnrealCloudDDC:ReferencesDbImplementation", "Scylla"),
        };
    }

    protected override async Task Seed(IServiceProvider provider)
    {
        string s3BucketName = $"tests-{TestNamespaceName}";

        _s3 = provider.GetService<IAmazonS3>();
        Assert.IsNotNull(_s3);
        if (await _s3!.DoesS3BucketExistAsync(s3BucketName))
        {
            // if we have failed to run the cleanup for some reason we run it now
            await Teardown(provider);
        }

        await _s3.PutBucketAsync(s3BucketName);
        await _s3.PutObjectAsync(new PutObjectRequest { BucketName = s3BucketName, Key = BlobIdentifier.FromBlobLocator(SmallFileLocator).AsS3Key(), ContentBody = SmallFileContents });
    }

    protected override async Task Teardown(IServiceProvider provider)
    {
        string s3BucketName = $"tests-{TestNamespaceName}";
        ListObjectsResponse response = await _s3!.ListObjectsAsync(s3BucketName);
        List<KeyVersion> objectKeys = response.S3Objects.Select(o => new KeyVersion { Key = o.Key }).ToList();
        await _s3.DeleteObjectsAsync(new DeleteObjectsRequest { BucketName = s3BucketName, Objects = objectKeys });

        await _s3.DeleteBucketAsync(s3BucketName);
    }
}

[TestClass]
public class MongoBundlesTests : BundlesTests
{
    private IAmazonS3? _s3;
    protected override IEnumerable<KeyValuePair<string, string>> GetSettings()
    {
        return new[]
        {
            new KeyValuePair<string, string>("UnrealCloudDDC:StorageImplementations:0", UnrealCloudDDCSettings.StorageBackendImplementations.S3.ToString()),
            new KeyValuePair<string, string>("S3:BucketName", $"tests-{TestNamespaceName}"),
            new KeyValuePair<string, string>("UnrealCloudDDC:BlobIndexImplementation", "Mongo"),
            new KeyValuePair<string, string>("UnrealCloudDDC:ReferencesDbImplementation", "Mongo"),
        };
    }

    protected override async Task Seed(IServiceProvider provider)
    {
        string s3BucketName = $"tests-{TestNamespaceName}";

        _s3 = provider.GetService<IAmazonS3>();
        Assert.IsNotNull(_s3);
        if (await _s3!.DoesS3BucketExistAsync(s3BucketName))
        {
            // if we have failed to run the cleanup for some reason we run it now
            await Teardown(provider);
        }

        await _s3.PutBucketAsync(s3BucketName);
        await _s3.PutObjectAsync(new PutObjectRequest { BucketName = s3BucketName, Key = BlobIdentifier.FromBlobLocator(SmallFileLocator).AsS3Key(), ContentBody = SmallFileContents });
    }

    protected override async Task Teardown(IServiceProvider provider)
    {
        string s3BucketName = $"tests-{TestNamespaceName}";
        ListObjectsResponse response = await _s3!.ListObjectsAsync(s3BucketName);
        List<KeyVersion> objectKeys = response.S3Objects.Select(o => new KeyVersion { Key = o.Key }).ToList();
        await _s3.DeleteObjectsAsync(new DeleteObjectsRequest { BucketName = s3BucketName, Objects = objectKeys });

        await _s3.DeleteBucketAsync(s3BucketName);
    }
}

public abstract class BundlesTests
{
    protected TestServer? Server { get; set; }
    protected NamespaceId TestNamespaceName { get; } = new NamespaceId("test-namespace-bundle");

    private HttpClient? _httpClient;

    protected const string SmallFileContents = "Small file contents";

    protected BlobLocator SmallFileLocator { get; } = BlobLocator.Create(HostId.Empty);

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
            .UseStartup<JupiterStartup>()
        );
        _httpClient = server.CreateClient();
        Server = server;

        // Seed storage
        await Seed(Server.Services);
    }

    protected abstract IEnumerable<KeyValuePair<string, string>> GetSettings();

    protected abstract Task Seed(IServiceProvider serverServices);
    protected abstract Task Teardown(IServiceProvider serverServices);

    [TestCleanup]
    public async Task MyTeardown()
    {
        await Teardown(Server!.Services);
    }

    [TestMethod]
    public async Task GetFile()
    {
        HttpResponseMessage result = await _httpClient!.GetAsync(new Uri($"api/v1/storage/{TestNamespaceName}/blobs/{SmallFileLocator.ToString().ToLower()}", UriKind.Relative));
        Assert.AreEqual(HttpStatusCode.Redirect, result.StatusCode);

        using HttpClient httpClient = new HttpClient();
        HttpResponseMessage redirectResult = await httpClient!.GetAsync(result.Headers.Location);
        redirectResult.EnsureSuccessStatusCode();
        string content = await redirectResult.Content.ReadAsStringAsync();
        Assert.AreEqual(SmallFileContents, content);
    }

    [TestMethod]
    public async Task PutSmallBlobDirectly()
    {
        byte[] payload = Encoding.ASCII.GetBytes("I am a small blob");
        using ByteArrayContent requestContent = new ByteArrayContent(payload);
        requestContent.Headers.ContentType = new MediaTypeHeaderValue("application/octet-stream");
        requestContent.Headers.ContentLength = payload.Length;

        using MultipartFormDataContent multipartContent = new MultipartFormDataContent();
        multipartContent.Add(requestContent, "file", "smallBlobContent.dat");

        HttpResponseMessage result = await _httpClient!.PostAsync(new Uri($"api/v1/storage/{TestNamespaceName}/blobs", UriKind.Relative), multipartContent);

        result.EnsureSuccessStatusCode();
    }

    [TestMethod]
    public async Task PutSmallBlobRedirect()
    {
        Assert.Inconclusive("Disabled as redirect uploads is currently disabled");
        byte[] payload = Encoding.ASCII.GetBytes("I am also a small blob");

        HttpResponseMessage result = await _httpClient!.PostAsync(new Uri($"api/v1/storage/{TestNamespaceName}/blobs", UriKind.Relative), null);
        /*Assert.AreEqual(HttpStatusCode.Redirect, result.StatusCode);*/

        result.EnsureSuccessStatusCode();
        WriteBlobResponse? response = await result.Content.ReadFromJsonAsync<WriteBlobResponse>();
        Assert.IsNotNull(response);
        //Assert.IsTrue(response.SupportsRedirects);
        Assert.IsNotNull(response.UploadUrl);

        Uri redirectUri = response.UploadUrl;
        using ByteArrayContent requestContent = new ByteArrayContent(payload);

        using HttpClient httpClient = new HttpClient();
        HttpResponseMessage redirectResult = await httpClient!.PutAsync(redirectUri, requestContent);
        string s = await redirectResult.Content.ReadAsStringAsync();
        redirectResult.EnsureSuccessStatusCode();
    }

    [TestMethod]
    public async Task PutGetRef()
    {
        IoHash targetHash = IoHash.Compute(Encoding.ASCII.GetBytes(SmallFileContents));
        RefName refName = new RefName("this-is-a-ref");
        int exportIdx = 1;
        using JsonContent requestContent = JsonContent.Create(new WriteRefRequest { Blob = SmallFileLocator, ExportIdx = exportIdx, Hash = targetHash, });
        HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v1/storage/{TestNamespaceName}/refs/{refName}", UriKind.Relative), requestContent);
        result.EnsureSuccessStatusCode();

        HttpResponseMessage getResult = await _httpClient!.GetAsync(new Uri($"api/v1/storage/{TestNamespaceName}/refs/{refName}", UriKind.Relative));
        getResult.EnsureSuccessStatusCode();
        ReadRefResponse? getResponse = await getResult.Content.ReadFromJsonAsync<ReadRefResponse>();
        Assert.IsNotNull(getResponse);

        Assert.AreEqual(targetHash, getResponse.Hash);
        Assert.AreEqual(SmallFileLocator, getResponse.Blob);
        Assert.AreEqual(exportIdx, getResponse.ExportIdx);
        Assert.AreEqual($"/api/v1/storage/test-namespace-bundle/nodes/{SmallFileLocator}?export={exportIdx}", getResponse.Link);
    }

    [TestMethod]
    public async Task PutGetBundle()
    {
        Bundle manualBundle = CreateBundleManually();
        byte[] payload = manualBundle.AsSequence().ToArray();
        using ByteArrayContent requestContent = new ByteArrayContent(payload);
        requestContent.Headers.ContentType = new MediaTypeHeaderValue("application/octet-stream");
        requestContent.Headers.ContentLength = payload.Length;

        using MultipartFormDataContent multipartContent = new MultipartFormDataContent();
        multipartContent.Add(requestContent, "file", "bundle.dat");

        HttpResponseMessage result = await _httpClient!.PostAsync(new Uri($"api/v1/storage/{TestNamespaceName}/blobs", UriKind.Relative), multipartContent);
        result.EnsureSuccessStatusCode();

        WriteBlobResponse? writeBlobResponse = await result.Content.ReadFromJsonAsync<WriteBlobResponse>();
        Assert.IsNotNull(writeBlobResponse);
        BlobLocator bundleLocator = writeBlobResponse.Blob;

        HttpResponseMessage getResult = await _httpClient!.GetAsync(new Uri($"api/v1/storage/{TestNamespaceName}/bundles/{bundleLocator}", UriKind.Relative));
        getResult.EnsureSuccessStatusCode();
    }
    
    [TestMethod]
    public async Task TestTreeAsync()
    {
        RefName rootRefName = new RefName("test");
        RefName leafRefName = new RefName("leaf");
        HttpMessageHandler httpMessageHandler = Server!.CreateHandler();

        HttpStorageClient blobStore = new HttpStorageClient(() => new HttpClient(httpMessageHandler) {BaseAddress = new Uri(Server.BaseAddress, $"api/v1/storage/{TestNamespaceName}/")}!, () => null!, null, NullLogger.Instance);
        await SeedTreeAsync(blobStore, rootRefName, leafRefName, new BundleOptions { MaxBlobSize = 1 });

        IBlobIndex blobIndex = Server.Services.GetService<IBlobIndex>()!;

        HttpResponseMessage getResultRoot = await _httpClient!.GetAsync(new Uri($"api/v1/storage/{TestNamespaceName}/refs/{rootRefName.Text}", UriKind.Relative));
        getResultRoot.EnsureSuccessStatusCode();
        ReadRefResponse? getResponseRoot = await getResultRoot.Content.ReadFromJsonAsync<ReadRefResponse>();
        Assert.IsNotNull(getResponseRoot);
        BlobIdentifier rootBlob = BlobIdentifier.FromBlobLocator(getResponseRoot.Blob);

        IAsyncEnumerable<BaseBlobReference> referencesEnumerable = blobIndex.GetBlobReferences(TestNamespaceName, rootBlob);
        BaseBlobReference[] references = await referencesEnumerable.ToArrayAsync();
        Assert.AreEqual(1, references.Length); // only the ref depends on the root node

        HttpResponseMessage getResultLeaf = await _httpClient!.GetAsync(new Uri($"api/v1/storage/{TestNamespaceName}/refs/{leafRefName.Text}", UriKind.Relative));
        getResultLeaf.EnsureSuccessStatusCode();
        ReadRefResponse? getResponseLeaf = await getResultLeaf.Content.ReadFromJsonAsync<ReadRefResponse>();
        Assert.IsNotNull(getResponseLeaf);
        BlobIdentifier node1Blob = BlobIdentifier.FromBlobLocator(getResponseLeaf.Blob);

        // node 1
        BaseBlobReference[] node1References = await blobIndex.GetBlobReferences(TestNamespaceName, node1Blob).ToArrayAsync();
        Assert.AreEqual(2, node1References.Length); // this has 2 incoming references, one from its import and one from the leafRef

        BlobIdentifier node2Blob = ((BlobToBlobReference)node1References.First()).Blob;
        // node 2
        BaseBlobReference[] node2References = await blobIndex.GetBlobReferences(TestNamespaceName, node2Blob).ToArrayAsync();
        Assert.AreEqual(1, node2References.Length);

        BlobIdentifier node3Blob = ((BlobToBlobReference)node2References.First()).Blob;
        // node 3
        BaseBlobReference[] node3References = await blobIndex.GetBlobReferences(TestNamespaceName, node3Blob).ToArrayAsync();
        Assert.AreEqual(1, node3References.Length);

        BlobIdentifier expectedRootNode = ((BlobToBlobReference)node3References.First()).Blob;
        Assert.AreEqual(rootBlob, expectedRootNode);
    }

    static Bundle CreateBundleManually()
    {
        ArrayMemoryWriter payloadWriter = new ArrayMemoryWriter(200);
        payloadWriter.WriteString("Hello world");
        byte[] payload = payloadWriter.WrittenMemory.ToArray();

        List<BlobType> types = new List<BlobType>();
        types.Add(new BlobType(Guid.Parse("F63606D4-5DBB-4061-A655-6F444F65229E"), 1));

        List<BundleExport> exports = new List<BundleExport>();
        exports.Add(new BundleExport(0, IoHash.Compute(payload), 0, 0, payload.Length, Array.Empty<BundleExportRef>()));

        List<BundlePacket> packets = new List<BundlePacket>();
        packets.Add(new BundlePacket(BundleCompressionFormat.None, 0, payload.Length, payload.Length));

        BundleHeader header = BundleHeader.Create(types, Array.Empty<BlobLocator>(), exports, packets);
        return new Bundle(header, new List<ReadOnlyMemory<byte>> { payload });
    }

    [NodeType("{F63606D4-5DBB-4061-A655-6F444F65229F}")]
    class SimpleNode : Node
    {
        public ReadOnlySequence<byte> Data { get; }
        public IReadOnlyList<NodeRef<SimpleNode>> Refs { get; }

        public SimpleNode(ReadOnlySequence<byte> data, IReadOnlyList<NodeRef<SimpleNode>> refs)
        {
            Data = data;
            Refs = refs;
        }

        public SimpleNode(NodeReader reader)
        {
            Data = new ReadOnlySequence<byte>(reader.ReadVariableLengthBytes());
            Refs = reader.ReadVariableLengthArray(() => reader.ReadNodeRef<SimpleNode>());
        }

        public override void Serialize(NodeWriter writer)
        {
            writer.WriteVariableLengthBytes(Data);
            writer.WriteVariableLengthArray(Refs, x => writer.WriteNodeRef(x));
        }
    }

    static async Task SeedTreeAsync(BundleStorageClient store, RefName rootRefName, RefName leafRefName, BundleOptions options)
    {
        // Generate a tree
        {
            await using IStorageWriter writer = store.CreateWriter(new RefName("test"), options);

            SimpleNode node1 = new SimpleNode(new ReadOnlySequence<byte>(new byte[] { 1 }), Array.Empty<NodeRef<SimpleNode>>());
            SimpleNode node2 = new SimpleNode(new ReadOnlySequence<byte>(new byte[] { 2 }), new[] { new NodeRef<SimpleNode>(await writer.WriteNodeAsync(node1)) });
            SimpleNode node3 = new SimpleNode(new ReadOnlySequence<byte>(new byte[] { 3 }), new[] { new NodeRef<SimpleNode>(await writer.WriteNodeAsync(node2)) });
            SimpleNode node4 = new SimpleNode(new ReadOnlySequence<byte>(new byte[] { 4 }), Array.Empty<NodeRef<SimpleNode>>());

            SimpleNode root = new SimpleNode(new ReadOnlySequence<byte>(new byte[] { 5 }), new[] { new NodeRef<SimpleNode>(await writer.WriteNodeAsync(node4)), new NodeRef<SimpleNode>(await writer.WriteNodeAsync(node3)) });

            await store.WriteNodeAsync(rootRefName, root);
            await store.WriteNodeAsync(leafRefName, node1);
        }
    }
}
