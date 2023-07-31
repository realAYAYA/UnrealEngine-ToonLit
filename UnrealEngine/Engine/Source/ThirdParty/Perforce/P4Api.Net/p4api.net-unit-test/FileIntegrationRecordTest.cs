using Perforce.P4;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using NLog;

namespace p4api.net.unit.test
{
    /// <summary>
    ///This is a test class for FileIntegrationRecordTest and is intended
    ///to contain all FileIntegrationRecordTest Unit Tests
    ///</summary>
	[TestClass()]
	public class FileIntegrationRecordTest
	{
        private static Logger logger = LogManager.GetCurrentClassLogger();
        public TestContext TestContext { get; set; }

        [TestInitialize]
        public void SetupTest()
        {
            Utilities.LogTestStart(TestContext);
        }
        [TestCleanup]
        public void CleanupTest()
        {
            Utilities.LogTestFinish(TestContext);
        }

        #region Additional test attributes
        // 
        //You can use the following additional attributes as you write your tests:
        //
        //Use ClassInitialize to run code before running the first test in the class
        //[ClassInitialize()]
        //public static void MyClassInitialize(TestContext testContext)
        //{
        //}
        //
        //Use ClassCleanup to run code after all tests in a class have run
        //[ClassCleanup()]
        //public static void MyClassCleanup()
        //{
        //}
        //
        //Use TestInitialize to run code before running each test
        //[TestInitialize()]
        //public void MyTestInitialize()
        //{
        //}
        //
        //Use TestCleanup to run code after each test has run
        //[TestCleanup()]
        //public void MyTestCleanup()
        //{
        //}
        //
        #endregion


        /// <summary>
        ///A test for ChangeId
        ///</summary>
        [TestMethod()]
		public void ChangeIdTest()
		{
			FileSpec fromfile = new FileSpec(new DepotPath("//depot/main/test"), new VersionRange(2, 4));
			FileSpec tofile = new FileSpec(new DepotPath("//depot/rel/test"), new VersionRange(2, 4));
			FileIntegrationRecord target = new FileIntegrationRecord(fromfile, tofile, IntegrateAction.BranchInto, 44444);
			int expected = 44444;
			int actual = target.ChangeId;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for FromFile
		///</summary>
		[TestMethod()]
		public void FromFileTest()
		{
			FileSpec fromfile = new FileSpec(new DepotPath("//depot/main/test"), new VersionRange(2, 4));
			FileSpec tofile = new FileSpec(new DepotPath("//depot/rel/test"), new VersionRange(2, 4));
			FileIntegrationRecord target = new FileIntegrationRecord(fromfile, tofile, IntegrateAction.BranchInto, 44444);
			FileSpec expected = new FileSpec(new DepotPath("//depot/main/test"), new VersionRange(2, 4));
			FileSpec actual = target.FromFile;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for How
		///</summary>
		[TestMethod()]
		public void HowTest()
		{
			FileSpec fromfile = new FileSpec(new DepotPath("//depot/main/test"), new VersionRange(2, 4));
			FileSpec tofile = new FileSpec(new DepotPath("//depot/rel/test"), new VersionRange(2, 4));
			FileIntegrationRecord target = new FileIntegrationRecord(fromfile, tofile, IntegrateAction.BranchInto, 44444);
			IntegrateAction expected = IntegrateAction.BranchInto;
			IntegrateAction actual = target.How;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for ToFile
		///</summary>
		[TestMethod()]
		public void ToFileTest()
		{
			FileSpec fromfile = new FileSpec(new DepotPath("//depot/main/test"), new VersionRange(2, 4));
			FileSpec tofile = new FileSpec(new DepotPath("//depot/rel/test"), new VersionRange(2, 4));
			FileIntegrationRecord target = new FileIntegrationRecord(fromfile, tofile, IntegrateAction.BranchInto, 44444);
			FileSpec expected = new FileSpec(new DepotPath("//depot/rel/test"), new VersionRange(2, 4));
			FileSpec actual = target.ToFile;
			Assert.AreEqual(expected, actual);
		}
	}
}
