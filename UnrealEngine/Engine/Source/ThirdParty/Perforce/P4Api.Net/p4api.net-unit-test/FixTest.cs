using Perforce.P4;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using NLog;

namespace p4api.net.unit.test
{
    /// <summary>
    ///This is a test class for FixTest and is intended
    ///to contain all FixTest Unit Tests
    ///</summary>
	[TestClass()]
	public class FixTest
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
        ///A test for Action
        ///</summary>
        [TestMethod()]
		public void ActionTest()
		{
			Fix target = new Fix("job000001", 4, new DateTime(2011, 02, 04), "client_name", "user_name", "open", FixAction.Unfixed);
			FixAction expected = FixAction.Unfixed;
			FixAction actual = target.Action;
			Assert.AreEqual(expected, actual);
		}

		///// <summary>
		/////A test for ChangelistId
		/////</summary>
		[TestMethod()]
		public void ChangelistIdTest()
		{
			Fix target = new Fix("job000001", 4, new DateTime(2011, 02, 04), "client_name", "user_name", "open", FixAction.Unfixed);
			int expected = 4;
			int actual = target.ChangeId;
			Assert.AreEqual(expected, actual);
		}

		///// <summary>
		/////A test for ClientName
		/////</summary>
		[TestMethod()]
		public void ClientNameTest()
		{
			Fix target = new Fix("job000001", 4, new DateTime(2011, 02, 04), "client_name", "user_name", "open", FixAction.Unfixed);
			string expected = "client_name";
			string actual = target.ClientName;
			Assert.AreEqual(expected, actual);
		}

		///// <summary>
		/////A test for Date
		/////</summary>
		[TestMethod()]
		public void DateTest()
		{
			Fix target = new Fix("job000001", 4, new DateTime(2011, 02, 04), "client_name", "user_name", "open", FixAction.Unfixed);
			DateTime expected = new DateTime(2011, 02, 04);
			DateTime actual = target.Date;
			Assert.AreEqual(expected, actual);
		}

		///// <summary>
		/////A test for JobId
		/////</summary>
		[TestMethod()]
		public void JobIdTest()
		{
			Fix target = new Fix("job000001", 4, new DateTime(2011, 02, 04), "client_name", "user_name", "open", FixAction.Unfixed);
			string expected = "job000001";
			string actual = target.JobId;
			Assert.AreEqual(expected, actual);
		}

		///// <summary>
		/////A test for Status
		/////</summary>
		[TestMethod()]
		public void StatusTest()
		{
			Fix target = new Fix("job000001", 4, new DateTime(2011,02,04), "client_name", "user_name", "open", FixAction.Unfixed);
			string expected = "open";
			string actual = target.Status;
			Assert.AreEqual(expected, actual);
		}

		///// <summary>
		/////A test for UserName
		/////</summary>
		[TestMethod()]
		public void UserNameTest()
		{
			Fix target = new Fix("job000001", 4, new DateTime(2011, 02, 04), "client_name", "user_name", "open", FixAction.Unfixed);
			string expected = "user_name";
			string actual = target.UserName;
			Assert.AreEqual(expected, actual);
		}
	}
}
