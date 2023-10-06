// Copyright Epic Games, Inc. All Rights Reserved.

using System.Threading;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Server.Tests
{
	[TestClass]
    public class DatabaseRunnerTest
    {
        [TestMethod]
        public void RunMongoDbTest()
        {
	        using MongoDbRunnerLocal runner = new MongoDbRunnerLocal();
	        runner.Start();
            Thread.Sleep(100);
            runner.Stop();
        }
        
        [TestMethod]
        public void RunRedisTest()
        {
	        using RedisRunner runner = new RedisRunner();
	        runner.Start();
	        Thread.Sleep(100);
	        runner.Stop();
        }
    }
}