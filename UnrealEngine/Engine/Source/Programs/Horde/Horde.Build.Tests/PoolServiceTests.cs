// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using Horde.Build.Agents;
using Horde.Build.Agents.Pools;
using Horde.Build.Utilities;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Build.Tests
{
	using PoolId = StringId<IPool>;

	[TestClass]
    public class PoolServiceTests : TestSetup
    {
        private readonly Dictionary<string, string> _fixtureProps = new Dictionary<string, string>
        {
            {"foo", "bar"},
            {"lorem", "ipsum"}
        };

        public PoolServiceTests()
        {
        }

        private async Task<IPool> CreatePoolFixture(string name)
        {
            return await PoolService.CreatePoolAsync(name, properties: _fixtureProps);
        }

        [TestMethod]
        public async Task GetPoolTest()
        {
            Assert.IsNull(await PoolService.GetPoolAsync(new PoolId("this-does-not-exist")));

            IPool newPool = await CreatePoolFixture("create-pool");
			IPool? pool = await PoolService.GetPoolAsync(newPool.Id);
			Assert.IsNotNull(pool);
            Assert.AreEqual("create-pool", pool!.Id.ToString());
            Assert.AreEqual("create-pool", pool.Name);
            Assert.AreEqual(_fixtureProps.Count, pool.Properties.Count);
            Assert.AreEqual(_fixtureProps["foo"], pool.Properties["foo"]);
            Assert.AreEqual(_fixtureProps["lorem"], pool.Properties["lorem"]);
        }
        
        [TestMethod]
        public async Task GetPoolsTest()
        {
            await GetMongoServiceSingleton().Database.DropCollectionAsync("Pools");
            
            List<IPool> pools = await PoolService.GetPoolsAsync();
            Assert.AreEqual(pools.Count, 0);

			IPool pool0 = await CreatePoolFixture("multiple-pools-0");
			IPool pool1 = await CreatePoolFixture("multiple-pools-1");
            pools = await PoolService.GetPoolsAsync();
            Assert.AreEqual(pools.Count, 2);
            Assert.AreEqual(pools[0].Name, pool0.Name);
            Assert.AreEqual(pools[1].Name, pool1.Name);
        }
        
        [TestMethod]
        public async Task DeletePoolTest()
        {
            Assert.IsFalse(await PoolService.DeletePoolAsync(new PoolId("this-does-not-exist")));
            IPool pool = await CreatePoolFixture("pool-to-be-deleted");
            Assert.IsTrue(await PoolService.DeletePoolAsync(pool.Id));
        }
        
        [TestMethod]
        public async Task UpdatePoolTest()
        {
			string uniqueSuffix = Guid.NewGuid().ToString("N");
            IPool pool = await CreatePoolFixture($"update-pool-{uniqueSuffix}");
            Dictionary<string, string?> updatedProps = new Dictionary<string, string?>
            {
                {"foo", "bar"},
                {"lorem", null}, // This entry will get removed
                {"cookies", "yumyum"},
            };

            IPool? updatedPool = await PoolService.UpdatePoolAsync(pool, $"update-pool-new-name-{uniqueSuffix}", newProperties: updatedProps);
			Assert.IsNotNull(updatedPool);
            Assert.AreEqual(pool.Id, updatedPool!.Id);
            Assert.AreEqual($"update-pool-new-name-{uniqueSuffix}", updatedPool.Name);
            Assert.AreEqual(updatedPool.Properties.Count, 2);
            Assert.AreEqual(updatedProps["foo"], updatedPool.Properties["foo"]);
            Assert.AreEqual(updatedProps["cookies"], updatedPool.Properties["cookies"]);
        }
        
        [TestMethod]
        public async Task UpdatePoolCollectionTest()
        {
	        IPool pool = await CreatePoolFixture("update-pool-2");
	        await PoolCollection.TryUpdateAsync(pool, lastScaleUpTime: DateTime.UtcNow);
        }
    }
}