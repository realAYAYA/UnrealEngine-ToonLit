// Copyright Epic Games, Inc. All Rights Reserved.

using System.Threading;
using System.Threading.Tasks;
using Horde.Server.Accounts;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Server.Tests.Accounts
{
	[TestClass]
	public class AccountCollectionTests : TestSetup
	{
		[TestMethod]
		public async Task CaseInsensitiveAsync()
		{
			IAccountCollection accountCollection = ServiceProvider.GetRequiredService<IAccountCollection>();

			IAccount? account = await accountCollection.FindByLoginAsync("bob");
			Assert.IsNull(account);

			await accountCollection.CreateAdminAccountAsync("", CancellationToken.None);

			account = await accountCollection.FindByLoginAsync("admin");
			Assert.IsNotNull(account);

			IAccount? account2 = await accountCollection.FindByLoginAsync("Admin");
			Assert.IsNotNull(account2);
			Assert.AreEqual(account.Id, account2.Id);

			account = await accountCollection.CreateAsync(new CreateAccountOptions("Robert", "bob"));
			Assert.IsNotNull(account);

			account2 = await accountCollection.FindByLoginAsync("Bob");
			Assert.IsNotNull(account2);
			Assert.AreEqual(account.Id, account2.Id);

			account2 = await account2.UpdateAsync(new UpdateAccountOptions(Name: "ROBERT", Login: "BOB"));
			Assert.IsNotNull(account2);

			account2 = await accountCollection.FindByLoginAsync("bob");
			Assert.IsNotNull(account2);
			Assert.AreEqual(account.Id, account2.Id);

			await Assert.ThrowsExceptionAsync<LoginAlreadyTakenException>(() => accountCollection.CreateAsync(new CreateAccountOptions("Bill", "bob")));
			await Assert.ThrowsExceptionAsync<LoginAlreadyTakenException>(() => account2.UpdateAsync(new UpdateAccountOptions { Login = "admin" }));
		}
	}
}
