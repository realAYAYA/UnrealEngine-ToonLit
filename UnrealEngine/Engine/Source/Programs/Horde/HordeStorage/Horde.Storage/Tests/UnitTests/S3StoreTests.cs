// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Text;
using System.Threading.Tasks;
using Amazon.S3;
using Amazon.S3.Model;
using EpicGames.Horde.Storage;
using Horde.Storage.Implementation;
using Horde.Storage.Implementation.Blob;
using Jupiter;
using Jupiter.Common;
using Jupiter.Implementation;
using Microsoft.Extensions.Options;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Moq;

namespace Horde.Storage.UnitTests
{
    [TestClass]
    public class S3StoreTests
    {
        private readonly NamespaceId Namespace = new NamespaceId("foo");

        [TestMethod]
        public async Task PutBufferSuccess()
        {
            Mock<IAmazonS3> s3Mock = new Mock<IAmazonS3>();
            S3Settings settings = new S3Settings{BucketName = "tests-foo"};
            IOptionsMonitor<S3Settings> settingsMock = Mock.Of<IOptionsMonitor<S3Settings>>(_ => _.CurrentValue == settings);

            INamespacePolicyResolver namespacePolicyResolver = Mock.Of<INamespacePolicyResolver>(_ => _.GetPoliciesForNs(Namespace) ==
                                                  new NamespacePolicy
                                                  {
                                                      StoragePool = ""
                                                  });

            AmazonS3Store store = new AmazonS3Store(s3Mock.Object, settingsMock, Mock.Of<IBlobIndex>(), namespacePolicyResolver);
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
            S3Settings settings = new S3Settings{BucketName = "tests-foo"};
            IOptionsMonitor<S3Settings> settingsMock = Mock.Of<IOptionsMonitor<S3Settings>>(_ => _.CurrentValue == settings);
            
            INamespacePolicyResolver namespacePolicyResolver = Mock.Of<INamespacePolicyResolver>(_ => _.GetPoliciesForNs(Namespace) ==
                new NamespacePolicy
                {
                    StoragePool = "storagepool"
                });

            AmazonS3Store store = new AmazonS3Store(s3Mock.Object, settingsMock, Mock.Of<IBlobIndex>(), namespacePolicyResolver);
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
            S3Settings settings = new S3Settings{BucketName = "tests-foo"};
            IOptionsMonitor<S3Settings> settingsMock = Mock.Of<IOptionsMonitor<S3Settings>>(_ => _.CurrentValue == settings);
            INamespacePolicyResolver namespacePolicyResolver = Mock.Of<INamespacePolicyResolver>(_ => _.GetPoliciesForNs(Namespace) ==
                new NamespacePolicy
                {
                    StoragePool = ""
                });
            s3Mock.Setup(s3 => s3.PutObjectAsync(It.IsAny<PutObjectRequest>(), default)).Throws<Exception>();
            AmazonS3Store store = new AmazonS3Store(s3Mock.Object, settingsMock, Mock.Of<IBlobIndex>(), namespacePolicyResolver);
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

            S3Settings settings = new S3Settings{BucketName = "tests-foo"};
            IOptionsMonitor<S3Settings> settingsMock = Mock.Of<IOptionsMonitor<S3Settings>>(_ => _.CurrentValue == settings);

            INamespacePolicyResolver namespacePolicyResolver = Mock.Of<INamespacePolicyResolver>(_ => _.GetPoliciesForNs(Namespace) ==
                new NamespacePolicy
                {
                    StoragePool = ""
                });
            AmazonS3Store store = new AmazonS3Store(s3Mock.Object, settingsMock, Mock.Of<IBlobIndex>(), namespacePolicyResolver);
            await using BlobContents blobContents = await store.GetObject(Namespace, blob);

            s3Mock.Verify(s3 => s3.GetObjectAsync("tests-foo", blob.AsS3Key(), default));
        }

        [TestMethod]
        public async Task DeleteSuccess()
        {
            byte[] content = Encoding.ASCII.GetBytes("test content");
            BlobIdentifier blob = BlobIdentifier.FromBlob(content);
            Mock<IAmazonS3> s3Mock = new Mock<IAmazonS3>();
            S3Settings settings = new S3Settings{BucketName = "tests-foo"};
            IOptionsMonitor<S3Settings> settingsMock = Mock.Of<IOptionsMonitor<S3Settings>>(_ => _.CurrentValue == settings);

            INamespacePolicyResolver namespacePolicyResolver = Mock.Of<INamespacePolicyResolver>(_ => _.GetPoliciesForNs(Namespace) ==
                new NamespacePolicy
                {
                    StoragePool = ""
                });

            AmazonS3Store store = new AmazonS3Store(s3Mock.Object, settingsMock, Mock.Of<IBlobIndex>(), namespacePolicyResolver);
            Task task = store.DeleteObject(Namespace, blob);
            await task;

            s3Mock.Verify(s3 => s3.DeleteObjectAsync("tests-foo", blob.AsS3Key(), default));
        }
    }
}
