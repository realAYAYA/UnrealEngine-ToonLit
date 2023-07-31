// Copyright Epic Games, Inc. All Rights Reserved.

#if false
using PoolId = Horde.Build.Utilities.StringId<Horde.Build.Models.IPool>;
using Microsoft.Extensions.Options;

namespace Horde.Build.Tests
{
	using JobId = ObjectId<IJob>;

    /// <summary>
    /// Tests aimed to be run from an IDE while debugging against real databases
    /// Uncomment the [Ignore] tag to run
    /// </summary>
	[TestClass]
    public class DebugTest : DatabaseIntegrationTest
    {
        private async Task<TestSetup> GetTestSetupExternal()
        {
	        TestSetup TestSetup = ebugnew TestSetup(GetDatabaseServiceExternal());
	        await TestSetup.CreateFixture(true);
	        return TestSetup;
        }

        private DatabaseService GetDatabaseServiceExternal()
        {
	        Startup.ConfigureMongoDbClient();

	        if (DebugSettings.DbHostname.IsNullOrEmpty())
	        {
		        throw new Exception("Please set credentials in DebugSettings.cs");
	        }
	        
	        ServerSettings Ss = new ServerSettings();
	        Ss.DatabaseName = "Horde";
	        Ss.DatabaseConnectionString = $"mongodb://{DebugSettings.DbUsername}:{DebugSettings.DbPassword}@{DebugSettings.DbHostname}:27017/?ssl=true&replicaSet=rs0";
	        Ss.DatabaseReadOnlyMode = true;
            
	        ILoggerFactory LoggerFactory = new LoggerFactory();
	        ConsoleLoggerOptions LoggerOptions = new ConsoleLoggerOptions();
	        TestOptionsMonitor<ConsoleLoggerOptions> LoggerOptionsMon = new TestOptionsMonitor<ConsoleLoggerOptions>(LoggerOptions);
	        LoggerFactory.AddProvider(new ConsoleLoggerProvider(LoggerOptionsMon));

	        DatabaseService DatabaseService = new DatabaseService(Options.Create(Ss), LoggerFactory);
	        return DatabaseService;
        }
        
//        [TestMethod]
        [Ignore]
        public async Task Debug1()
        {
	        TestSetup TestSetup = await GetTestSetupExternal();
	        
	        HordeServer.Models.IJob? Job = await TestSetup.JobService.GetJobAsync(new JobId("5f98da1d98cf40000141344c"));
	        Console.WriteLine("Job is " + Job);
        }
        
//        [TestMethod]
        [Ignore]
        public async Task DebugFleetManager()
        {
	        TestSetup TestSetup = await GetTestSetup();

	        IPool Pool = await TestSetup.PoolCollection.AddAsync(new PoolId("ue4-dev-win"), "ue4-dev-win", null, null);
	        AwsFleetManager Manager = new AwsFleetManager(TestSetup.AgentCollection, TestSetup.ServiceProvider.GetRequiredService<ILogger<AwsFleetManager>>());

	        await Manager.ExpandPool(Pool, new List<IAgent>(), 1);
        }
       
//        [TestMethod]
        [Ignore]
        public async Task DebugAutoScaleService()
        {
	        TestSetup TestSetup = await GetTestSetupExternal();

	        AutoscaleService AutoscaleService = new AutoscaleService(GetDatabaseServiceExternal(), TestSetup.AgentCollection, TestSetup.PoolCollection,
		        TestSetup.LeaseCollection, new DefaultFleetManager(TestSetup.ServiceProvider.GetRequiredService<ILogger<DefaultFleetManager>>()),
				new NoOpDogStatsd(), TestSetup.ServiceProvider.GetRequiredService<ILogger<AutoscaleService>>());

	        await AutoscaleService.TickSharedOnlyForTestingAsync();
        }
    }
}
#endif
