using Perforce.P4;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using NLog;

namespace p4api.net.unit.test
{
    /// <summary>
    ///This is a test class for DepotFileDiffTest and is intended
    ///to contain all DepotFileDiffTest Unit Tests
    ///</summary>
    [TestClass()]
    public class DepotFileDiffTest
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

        static DiffType type = DiffType.Content;
		static FileSpec leftfile = new FileSpec(new DepotPath("//depot/main/readme.txt"), new Revision(4));
		static FileSpec rightfile = new FileSpec(new DepotPath("//depot/release/readme.txt"), new Revision(1));
		static string diff = "2c2/r/n< bye---/r/n> buy";

		static DepotFileDiff target = null;

		static void setTarget()
		{
			target = new DepotFileDiff(
				type, leftfile, rightfile, diff);
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
        ///A test for Diff
        ///</summary>
        [TestMethod()]
        public void DiffTest()
        {
			string expected = "4c4/r/n< hello---/r/n> help";
			setTarget();
			Assert.AreEqual(target.Diff, "2c2/r/n< bye---/r/n> buy");
            target.Diff = expected;
            string actual = target.Diff;
            Assert.AreEqual(expected, actual);
        }

        /// <summary>
        ///A test for LeftFile
        ///</summary>
        [TestMethod()]
        public void LeftFileTest()
		{
			FileSpec expected = new FileSpec(new DepotPath("//depot/release/readme.txt"), new Revision(1));
			setTarget();
			Assert.AreEqual(target.LeftFile, new FileSpec(new DepotPath("//depot/main/readme.txt"), new Revision(4)));
			target.LeftFile = expected;
			FileSpec actual = target.LeftFile;
			Assert.AreEqual(expected, actual);
		}

        /// <summary>
        ///A test for RightFile
        ///</summary>
        [TestMethod()]
        public void RightFileTest()
		{
			FileSpec expected = new FileSpec(new DepotPath("//depot/main/readme.txt"), new Revision(4));
			setTarget();
			Assert.AreEqual(target.RightFile, new FileSpec(new DepotPath("//depot/release/readme.txt"), new Revision(1)));
			target.RightFile = expected;
			FileSpec actual = target.RightFile;
			Assert.AreEqual(expected, actual);
		}

        /// <summary>
        ///A test for Type
        ///</summary>
        [TestMethod()]
        public void TypeTest()
		{
			DiffType expected = DiffType.FileType;
			setTarget();
			Assert.AreEqual(target.Type, DiffType.Content);
			target.Type = expected;
			DiffType actual = target.Type;
			Assert.AreEqual(expected, actual);
		}
    }
}
