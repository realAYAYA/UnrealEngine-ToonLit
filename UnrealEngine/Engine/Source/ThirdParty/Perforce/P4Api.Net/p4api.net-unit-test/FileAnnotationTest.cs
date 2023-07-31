using Perforce.P4;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using NLog;

namespace p4api.net.unit.test
{
    /// <summary>
    ///This is a test class for FileAnnotationTest and is intended
    ///to contain all FileAnnotationTest Unit Tests
    ///</summary>
	[TestClass()]
	public class FileAnnotationTest
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
        ///A test for File
        ///</summary>
        [TestMethod()]
		public void FileTest()
		{
			FileAnnotation target = new FileAnnotation(new FileSpec(new DepotPath("//depot/annotate.txt"),null), "this is the line");
			FileSpec expected = new FileSpec(new DepotPath("//depot/annotate.txt"), null, null, null);
			FileSpec actual = target.File;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for Line
		///</summary>
		[TestMethod()]
		public void LineTest()
		{
			FileAnnotation target = new FileAnnotation(new FileSpec(new DepotPath("//depot/annotate.txt"), null), "this is the line");
			string expected = "this is the line";
			string actual = target.Line;
			Assert.AreEqual(expected, actual);
		}
	}
}
