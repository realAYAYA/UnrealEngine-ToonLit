// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Text;
using System.Threading.Tasks;
using Amazon.S3;
using Amazon.S3.Model;
using EpicGames.Horde.Storage;
using Jupiter.Implementation;
using Jupiter.Implementation.Blob;
using Jupiter.Common;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Logging.Abstractions;
using Microsoft.Extensions.Options;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Moq;
using OpenTelemetry.Trace;

namespace Jupiter.UnitTests
{
    [TestClass]
    public class S3StoreTests
    {
        private readonly NamespaceId Namespace = new NamespaceId("foo");

        private ServiceProvider SetupProvider(Mock<IAmazonS3> s3Mock, string storagePool = "")
        {
            ServiceCollection serviceCollection = new ServiceCollection();

            serviceCollection.AddSingleton<IAmazonS3>(s3Mock.Object);

            S3Settings settings = new S3Settings{BucketName = "tests-foo"};
            IOptionsMonitor<S3Settings> settingsMock = Mock.Of<IOptionsMonitor<S3Settings>>(_ => _.CurrentValue == settings);
            serviceCollection.AddSingleton<IOptionsMonitor<S3Settings>>(settingsMock);

            INamespacePolicyResolver namespacePolicyResolver = Mock.Of<INamespacePolicyResolver>(_ => _.GetPoliciesForNs(Namespace) ==
                new NamespacePolicy
                {
                    StoragePool = storagePool
                });
            serviceCollection.AddSingleton<INamespacePolicyResolver>(namespacePolicyResolver);

            serviceCollection.AddSingleton<IBlobIndex>(Mock.Of<IBlobIndex>());
            serviceCollection.AddSingleton<Tracer>(TracerProvider.Default.GetTracer("Tests"));
            serviceCollection.AddSingleton<ILogger<AmazonS3Store>>(NullLogger<AmazonS3Store>.Instance);
            serviceCollection.AddSingleton<ILogger<AmazonStorageBackend>>(NullLogger<AmazonStorageBackend>.Instance);
            
            return serviceCollection.BuildServiceProvider();
        }

        [TestMethod]
        public async Task PutBufferSuccess()
        {
            Mock<IAmazonS3> s3Mock = new Mock<IAmazonS3>();

            ServiceProvider provider = SetupProvider(s3Mock);
            AmazonS3Store store = ActivatorUtilities.CreateInstance<AmazonS3Store>(provider);
            byte[] content = Encoding.ASCII.GetBytes("test content");
            BlobIdentifier blobIdentifier = BlobIdentifier.FromBlob(content);
            Task task = store.PutObject(Namespace, content.AsMemory(), blobIdentifier);
            await task;

            s3Mock.Verify( s3 => s3.DoesS3BucketExistAsync("tests-foo"));
            s3Mock.Verify( s3 => s3.PutBucketAsync(It.Is<PutBucketRequest>(p => p.BucketName =="tests-foo"), default));
            s3Mock.Verify(s3 =>
                s3.PutObjectAsync(It.Is<PutObjectRequest>(p => p.BucketName == "tests-foo" && p.Key == blobIdentifier.AsS3Key()), default));

            s3Mock.Verify( s3 => s3.PutPublicAccessBlockAsync(It.IsAny<PutPublicAccessBlockRequest>(), default));
            s3Mock.VerifyNoOtherCalls();
        }

        [TestMethod]
        public async Task PutBufferSuccessStoragePool()
        {
            Mock<IAmazonS3> s3Mock = new Mock<IAmazonS3>();
            ServiceProvider provider = SetupProvider(s3Mock, "storagepool");
            AmazonS3Store store = ActivatorUtilities.CreateInstance<AmazonS3Store>(provider);
            byte[] content = Encoding.ASCII.GetBytes("test content");
            BlobIdentifier blobIdentifier = BlobIdentifier.FromBlob(content);
            Task task = store.PutObject(Namespace, content.AsMemory(), blobIdentifier);
            await task;

            s3Mock.Verify( s3 => s3.DoesS3BucketExistAsync("tests-foo-storagepool"));
            s3Mock.Verify( s3 => s3.PutBucketAsync(It.Is<PutBucketRequest>(p => p.BucketName =="tests-foo-storagepool"), default));
            s3Mock.Verify(s3 =>
                s3.PutObjectAsync(It.Is<PutObjectRequest>(p => p.BucketName == "tests-foo-storagepool" && p.Key == blobIdentifier.AsS3Key()), default));

            s3Mock.Verify( s3 => s3.PutPublicAccessBlockAsync(It.IsAny<PutPublicAccessBlockRequest>(), default));
            s3Mock.VerifyNoOtherCalls();
        }

        [TestMethod]
        [ExpectedException(typeof(Exception))]
        public async Task PutBufferError()
        {
            byte[] content = Encoding.ASCII.GetBytes("test content");
            BlobIdentifier blob = BlobIdentifier.FromBlob(content);

            Mock<IAmazonS3> s3Mock = new Mock<IAmazonS3>();
            s3Mock.Setup(s3 => s3.PutObjectAsync(It.IsAny<PutObjectRequest>(), default)).Throws<Exception>();
            ServiceProvider provider = SetupProvider(s3Mock);
            AmazonS3Store store = ActivatorUtilities.CreateInstance<AmazonS3Store>(provider);
            Task task = store.PutObject(Namespace, content, blob);
            await task;

            s3Mock.Verify(s3 =>
                s3.PutObjectAsync(
                    It.Is<PutObjectRequest>(p => p.BucketName == "tests-foo" && p.FilePath == blob.AsS3Key() && p.InputStream == null),
                    default));
        }

        [TestMethod]
        public async Task GetSuccess()
        {
            byte[] content = Encoding.ASCII.GetBytes("test content");
            BlobIdentifier blob = BlobIdentifier.FromBlob(content);
            Mock<IAmazonS3> s3Mock = new Mock<IAmazonS3>();
            s3Mock.Setup(s3 => s3.GetObjectAsync("tests-foo", blob.AsS3Key(), default)).ReturnsAsync(Mock.Of<GetObjectResponse>());
            ServiceProvider provider = SetupProvider(s3Mock);
            AmazonS3Store store = ActivatorUtilities.CreateInstance<AmazonS3Store>(provider);
            await using BlobContents blobContents = await store.GetObject(Namespace, blob);

            s3Mock.Verify(s3 => s3.GetObjectAsync("tests-foo", blob.AsS3Key(), default));
        }

        [TestMethod]
        public async Task DeleteSuccess()
        {
            byte[] content = Encoding.ASCII.GetBytes("test content");
            BlobIdentifier blob = BlobIdentifier.FromBlob(content);
            
            Mock<IAmazonS3> s3Mock = new Mock<IAmazonS3>();
            ServiceProvider provider = SetupProvider(s3Mock);
            AmazonS3Store store = ActivatorUtilities.CreateInstance<AmazonS3Store>(provider);
            Task task = store.DeleteObject(Namespace, blob);
            await task;

            s3Mock.Verify(s3 => s3.DeleteObjectAsync("tests-foo", blob.AsS3Key(), default));
        }
    }
}
