using Perforce.P4;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using NLog;

namespace p4api.net.unit.test
{
    /// <summary>
    ///This is a test class for FileLineMatchTest and is intended
    ///to contain all FileLineMatchTest Unit Tests
    ///</summary>
	[TestClass()]
	public class FileLineMatchTest
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
        ///A test for FileSpec
        ///</summary>
        [TestMethod()]
		[DeploymentItem("p4api.net.dll")]
		public void FileSpecTest()
		{
			FileLineMatch target = new FileLineMatch(MatchType.After, "the matching line", 4,
				new FileSpec(new DepotPath("//depot/..."), new Revision(2)));
			FileSpec expected = new FileSpec(new ClientPath("//annotate/..."),
				new VersionRange(new LabelNameVersion("my_label"), new LabelNameVersion("my_old_label")));
			FileSpec actual;
			target.FileSpec = expected;
			actual = target.FileSpec;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for Line
		///</summary>
		[TestMethod()]
		[DeploymentItem("p4api.net.dll")]
		public void LineTest()
		{
			FileLineMatch target = new FileLineMatch(MatchType.After, "the matching line", 4,
				new FileSpec(new DepotPath("//depot/..."), new Revision(2)));
			string expected = "here is a line";
			string actual;
			target.Line = expected;
			actual = target.Line;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for LineNumber
		///</summary>
		[TestMethod()]
		[DeploymentItem("p4api.net.dll")]
		public void LineNumberTest()
		{
			FileLineMatch target = new FileLineMatch(MatchType.After, "the matching line", 4,
				new FileSpec(new DepotPath("//depot/..."), new Revision(2)));
			int expected = 1999;
			int actual;
			target.LineNumber = expected;
			actual = target.LineNumber;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for Type
		///</summary>
		[TestMethod()]
		[DeploymentItem("p4api.net.dll")]
		public void TypeTest()
		{
			FileLineMatch target = new FileLineMatch(MatchType.After, "the matching line", 4,
				new FileSpec(new DepotPath("//depot/..."), new Revision(2)));
			MatchType expected = MatchType.Before; 
			MatchType actual;
			target.Type = expected;
			actual = target.Type;
			Assert.AreEqual(expected, actual);
		}
	}
}
