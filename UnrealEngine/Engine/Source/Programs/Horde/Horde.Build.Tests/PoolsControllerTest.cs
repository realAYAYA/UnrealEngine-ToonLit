// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Threading.Tasks;
using Horde.Build.Agents;
using Horde.Build.Agents.Fleet;
using Horde.Build.Agents.Pools;
using Horde.Build.Utilities;
using Microsoft.AspNetCore.Mvc;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Build.Tests
{
	[TestClass]
    public class PoolsControllerTest : TestSetup
    {
        [TestMethod]
        public async Task GetPoolsTest()
        {
	        IPool pool1 = await PoolService.CreatePoolAsync("Pool1", properties: new() { {"foo", "bar"}, {"lorem", "ipsum"} });
	        ActionResult<List<object>> rawResult = await PoolsController.GetPoolsAsync();
	        Assert.AreEqual(1, rawResult.Value!.Count);
	        GetPoolResponse response = (rawResult.Value![0] as GetPoolResponse)!;
	        Assert.AreEqual(pool1.Id.ToString(), response.Id);
	        Assert.AreEqual(pool1.Name, response.Name);
	        Assert.AreEqual(pool1.SizeStrategy, response.SizeStrategy);
        }
        
        [TestMethod]
        public async Task CreatePoolsTest()
        {
	        CreatePoolRequest request = new CreatePoolRequest
	        {
		        Name = "Pool1",
		        ScaleOutCooldown = 111,
		        ScaleInCooldown = 222,
		        SizeStrategy = PoolSizeStrategy.JobQueue,
		        JobQueueSettings = new JobQueueSettingsMessage(new JobQueueSettings(0.35, 0.85))
	        };
	        
	        ActionResult<CreatePoolResponse> result = await PoolsController.CreatePoolAsync(request);
	        CreatePoolResponse response = result.Value!;
		        
	        IPool pool = (await PoolService.GetPoolAsync(new StringId<IPool>(response.Id)))!;
	        Assert.AreEqual(request.Name, pool.Name);
	        Assert.AreEqual(request.ScaleOutCooldown, (int)pool.ScaleOutCooldown!.Value.TotalSeconds);
	        Assert.AreEqual(request.ScaleInCooldown, (int)pool.ScaleInCooldown!.Value.TotalSeconds);
	        Assert.AreEqual(request.JobQueueSettings.ScaleOutFactor, pool.JobQueueSettings!.ScaleOutFactor, 0.0001);
	        Assert.AreEqual(request.JobQueueSettings.ScaleInFactor, pool.JobQueueSettings!.ScaleInFactor, 0.0001);
        }
        
        [TestMethod]
        public async Task UpdatePoolTest()
        {
	        IPool pool1 = await PoolService.CreatePoolAsync("Pool1", properties: new() { {"foo", "bar"}, {"lorem", "ipsum"} });

	        await PoolsController.UpdatePoolAsync(pool1.Id.ToString(), new UpdatePoolRequest
	        {
		        Name = "Pool1Modified",
		        SizeStrategy = PoolSizeStrategy.JobQueue,
		        JobQueueSettings = new JobQueueSettingsMessage { ScaleOutFactor = 25.0, ScaleInFactor = 0.3 }
	        });
	        
	        ActionResult<object> getResult = await PoolsController.GetPoolAsync(pool1.Id.ToString());
	        GetPoolResponse response = (getResult.Value! as GetPoolResponse)!;
	        Assert.AreEqual("Pool1Modified", response.Name);
	        Assert.AreEqual(PoolSizeStrategy.JobQueue, response.SizeStrategy);
	        Assert.AreEqual(25.0, response.JobQueueSettings!.ScaleOutFactor);
	        Assert.AreEqual(0.3, response.JobQueueSettings!.ScaleInFactor);
        }
    }
}