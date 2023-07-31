// Copyright Epic Games, Inc. All Rights Reserved.

using System.Threading;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Build.Tests
{
	[TestClass]
    public class DatabaseRunnerTest
    {
        [TestMethod]
        public void RunMongoDbTest()
        {
	        MongoDbRunnerLocal runner = new MongoDbRunnerLocal();
	        runner.Start();
            Thread.Sleep(100);
            runner.Stop();
        }
        
        [TestMethod]
        public void RunRedisTest()
        {
	        RedisRunner runner = new RedisRunner();
	        runner.Start();
	        Thread.Sleep(100);
	        runner.Stop();
        }
    }
}