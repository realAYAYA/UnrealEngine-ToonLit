using Perforce.P4;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using NLog;

namespace p4api.net.unit.test
{
    /// <summary>
    ///This is a test class for ProtectionTableTest and is intended
    ///to contain all ProtectionTableTest Unit Tests
    ///</summary>
	[TestClass()]
	public class ProtectionTableTest
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
        ///A test for ProtectionTable Constructor
        ///</summary>
        [TestMethod()]
		public void ProtectionTableConstructorTest()
		{
			ProtectionTable target = new ProtectionTable(new ProtectionEntry(ProtectionMode.Super, EntryType.User, " ", " ", " ", false));
			ProtectionEntry Entry1 = new ProtectionEntry(ProtectionMode.Admin, EntryType.Group, "admin_user", "win-admin-host", "//...", false);
			ProtectionEntry Entry2 = new ProtectionEntry(ProtectionMode.Read, EntryType.User, "read_user", "win-user-host", "//depot/test/...", false);
			target.Add(Entry1);
			target.Add(Entry2);
			Assert.AreEqual(Entry1, target[0]);
			Assert.AreEqual(Entry2, target[1]);
		}
	}
}
