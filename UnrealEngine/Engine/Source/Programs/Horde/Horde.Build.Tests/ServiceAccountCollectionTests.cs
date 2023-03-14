// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Threading.Tasks;
using Horde.Build.Server;
using Horde.Build.Users;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using MongoDB.Bson;

namespace Horde.Build.Tests
{
	[TestClass]
	public class ServiceAccountCollectionTests : DatabaseIntegrationTest
	{
		private readonly IServiceAccountCollection _serviceAccounts;
		private readonly IServiceAccount _serviceAccount;
		
		public ServiceAccountCollectionTests()
		{
			MongoService mongoService = GetMongoServiceSingleton();
			_serviceAccounts = new ServiceAccountCollection(mongoService);
			_serviceAccount = _serviceAccounts.AddAsync(ObjectId.GenerateNewId().ToString(), new List<string> {"myclaim###myvalue"}, "mydesc").Result;
		}

		[TestMethod]
		public async Task Add()
		{
			IServiceAccount sa = await _serviceAccounts.AddAsync("addToken", new List<string> {"myclaim###myvalue"}, "mydesc");
			Assert.AreEqual("addToken", sa.SecretToken);
			Assert.AreEqual(1, sa.GetClaims().Count);
			Assert.AreEqual("myvalue", sa.GetClaims()[0].Value);
			Assert.IsTrue(sa.Enabled);
			Assert.AreEqual("mydesc", sa.Description);
		}
		
		[TestMethod]
		public async Task Get()
		{
			IServiceAccount sa = (await _serviceAccounts.GetAsync(_serviceAccount.Id))!;
			Assert.AreEqual(_serviceAccount, sa);
		}
		
		[TestMethod]
		public async Task GetBySecretToken()
		{
			IServiceAccount sa = (await _serviceAccounts.GetBySecretTokenAsync(_serviceAccount.SecretToken))!;
			Assert.AreEqual(_serviceAccount, sa);
		}
		
		[TestMethod]
		public async Task Update()
		{
			List<string> newClaims = new List<string> {{"newclaim1###newvalue1"}, {"newclaim2###newvalue2"}};
			await _serviceAccounts.UpdateAsync(_serviceAccount.Id, "newtoken", newClaims, false, "newdesc");
			IServiceAccount getSa = (await _serviceAccounts.GetAsync(_serviceAccount.Id))!;
			
			Assert.AreEqual("newtoken", getSa.SecretToken);
			Assert.AreEqual(2, getSa.GetClaims().Count);
			Assert.AreEqual("newvalue1", getSa.GetClaims()[0].Value);
			Assert.AreEqual("newvalue2", getSa.GetClaims()[1].Value);
			Assert.AreEqual(false, getSa.Enabled);
			Assert.AreEqual("newdesc", getSa.Description);
		}
		
		[TestMethod]
		public async Task Delete()
		{
			await _serviceAccounts.DeleteAsync(_serviceAccount.Id);
			IServiceAccount? result = await _serviceAccounts.GetAsync(_serviceAccount.Id);
			Assert.IsNull(result);
		}
	}
}