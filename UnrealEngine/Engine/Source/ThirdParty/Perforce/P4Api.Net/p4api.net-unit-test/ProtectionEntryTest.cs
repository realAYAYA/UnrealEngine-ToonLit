using Perforce.P4;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using NLog;

namespace p4api.net.unit.test
{
    /// <summary>
    ///This is a test class for ProtectionEntryTest and is intended
    ///to contain all ProtectionEntryTest Unit Tests
    ///</summary>
	[TestClass()]
	public class ProtectionEntryTest
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
        ///A test for GroupOrUserName
        ///</summary>
        [TestMethod()]
		public void GroupOrUserNameTest()
		{
			ProtectionMode mode = new ProtectionMode(); // TODO: Initialize to an appropriate value
			EntryType type = EntryType.User;
			string grouporusername = "user_bob"; // only adding username
			string host = string.Empty; // TODO: Initialize to an appropriate value
			string path = string.Empty; // TODO: Initialize to an appropriate value
			ProtectionEntry target = new ProtectionEntry(mode, type, grouporusername, host, path, false); // TODO: Initialize to an appropriate value
			string expected = string.Empty; // TODO: Initialize to an appropriate value
			string actual;
			target.Name = expected;
			actual = target.Name;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for Host
		///</summary>
		[TestMethod()]
		public void HostTest()
		{
			ProtectionMode mode = new ProtectionMode(); // TODO: Initialize to an appropriate value
			EntryType type = EntryType.User;
			string grouporusername = string.Empty; // TODO: Initialize to an appropriate value
			string host = "win-user_bob"; // only adding host
			string path = string.Empty; // TODO: Initialize to an appropriate value
			ProtectionEntry target = new ProtectionEntry(mode, type, grouporusername, host, path, false); // TODO: Initialize to an appropriate value
			string expected = string.Empty; // TODO: Initialize to an appropriate value
			string actual;
			target.Host = expected;
			actual = target.Host;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for IsUserEntry
		///</summary>
		[TestMethod()]
		public void TypeTest()
		{
			ProtectionMode mode = new ProtectionMode(); // TODO: Initialize to an appropriate value
			EntryType type = EntryType.User;
			string grouporusername = string.Empty; // TODO: Initialize to an appropriate value
			string host = string.Empty; // TODO: Initialize to an appropriate value
			string path = string.Empty; // TODO: Initialize to an appropriate value
			ProtectionEntry target = new ProtectionEntry(mode, type, grouporusername, host, path, false); // TODO: Initialize to an appropriate value
			EntryType expected = EntryType.User;
			EntryType actual;
			target.Type = expected;
			actual = target.Type;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for Mode
		///</summary>
		[TestMethod()]
		public void ModeTest()
		{
			ProtectionMode mode = new ProtectionMode(); // TODO: Initialize to an appropriate value
			EntryType type = new EntryType();
			string grouporusername = string.Empty; // TODO: Initialize to an appropriate value
			string host = string.Empty; // TODO: Initialize to an appropriate value
			string path = string.Empty; // TODO: Initialize to an appropriate value
			ProtectionEntry target = new ProtectionEntry(mode, type, grouporusername, host, path, false); // TODO: Initialize to an appropriate value
			ProtectionMode expected = ProtectionMode.Super; // only adding protection mode super
			ProtectionMode actual;
			target.Mode = expected;
			actual = target.Mode;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for Path
		///</summary>
		[TestMethod()]
		public void PathTest()
		{
			ProtectionMode mode = new ProtectionMode(); // TODO: Initialize to an appropriate value
			EntryType type = new EntryType();
			string grouporusername = string.Empty; // TODO: Initialize to an appropriate value
			string host = string.Empty; // TODO: Initialize to an appropriate value
			string path = string.Empty; // TODO: Initialize to an appropriate value
			ProtectionEntry target = new ProtectionEntry(mode, type, grouporusername, host, path, false); // TODO: Initialize to an appropriate value
			string expected = "//..."; // only adding wide open path
			string actual;
			target.Path = expected;
			actual = target.Path;
			Assert.AreEqual(expected, actual);
		}
	}
}
