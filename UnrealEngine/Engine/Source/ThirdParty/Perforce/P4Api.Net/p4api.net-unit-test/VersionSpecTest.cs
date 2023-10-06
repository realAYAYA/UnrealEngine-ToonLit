using Perforce.P4;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using NLog;

namespace p4api.net.unit.test
{
    /// <summary>
    ///This is a test class for VersionSpecTest and is intended
    ///to contain all VersionSpecTest Unit Tests
    ///</summary>
	[TestClass()]
	public class VersionSpecTest
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
        ///A test for VersionSpec
        ///</summary>
        [TestMethod()]
		public void VersionSpecTest1()
		{
			VersionSpec left = new VersionRange(1,2);
			VersionSpec rightpos = new VersionRange(1, 2);
			VersionSpec rightneg1 = new VersionRange(1, 4);
			VersionSpec rightneg2 = new Revision(1);
			VersionSpec rightnull = null;

			Assert.IsTrue(left.Equals(rightpos));
			Assert.IsFalse(left.Equals(rightneg1));
			Assert.IsFalse(left.Equals(rightneg2));
			Assert.IsFalse(left.Equals(rightnull));

			left = new HeadRevision();
			rightpos = new HeadRevision();
			rightneg1 = new HaveRevision();
			rightnull = null;

			Assert.IsTrue(left.Equals(rightpos));
			Assert.IsFalse(left.Equals(rightneg1));
			Assert.IsFalse(left.Equals(rightnull));

			left = new HaveRevision();
			rightpos = new HaveRevision();
			rightneg1 = new HeadRevision();
			rightnull = null;

			Assert.IsTrue(left.Equals(rightpos));
			Assert.IsFalse(left.Equals(rightneg1));
			Assert.IsFalse(left.Equals(rightnull));

			left = new NoneRevision();
			rightpos = new NoneRevision();
			rightneg1 = new HaveRevision();
			rightnull = null;

			Assert.IsTrue(left.Equals(rightpos));
			Assert.IsFalse(left.Equals(rightneg1));
			Assert.IsFalse(left.Equals(rightnull));

			left = new Revision(1);
			rightpos = new Revision(1);
			rightneg1 = new Revision(3);
			rightneg2 = new VersionRange(1, 4);
			rightnull = null;

			Assert.IsTrue(left.Equals(rightpos));
			Assert.IsFalse(left.Equals(rightneg1));
			Assert.IsFalse(left.Equals(rightneg2));
			Assert.IsFalse(left.Equals(rightnull));

			left = new DateTimeVersion(DateTime.MinValue);
			rightpos = new DateTimeVersion(DateTime.MinValue);
			rightneg1 = new DateTimeVersion(DateTime.MaxValue);
			rightneg2 = new Revision(3);
			rightnull = null;

			Assert.IsTrue(left.Equals(rightpos));
			Assert.IsFalse(left.Equals(rightneg1));
			Assert.IsFalse(left.Equals(rightneg2));
			Assert.IsFalse(left.Equals(rightnull));

			left = new LabelNameVersion("label_name");
			rightpos = new LabelNameVersion("label_name");
			rightneg1 = new LabelNameVersion("wrong_label_name");
			rightneg2 = new Revision(3);
			rightnull = null;

			Assert.IsTrue(left.Equals(rightpos));
			Assert.IsFalse(left.Equals(rightneg1));
			Assert.IsFalse(left.Equals(rightneg2));
			Assert.IsFalse(left.Equals(rightnull));

			left = new ChangelistIdVersion(44444);
			rightpos = new ChangelistIdVersion(44444);
			rightneg1 = new ChangelistIdVersion(88888);
			rightneg2 = new Revision(3);
			rightnull = null;

			Assert.IsTrue(left.Equals(rightpos));
			Assert.IsFalse(left.Equals(rightneg1));
			Assert.IsFalse(left.Equals(rightneg2));
			Assert.IsFalse(left.Equals(rightnull));

			left = new ClientNameVersion("client_name");
			rightpos = new ClientNameVersion("client_name");
			rightneg1 = new ClientNameVersion("wrong_client_name");
			rightneg2 = new Revision(3);
			rightnull = null;

			Assert.IsTrue(left.Equals(rightpos));
			Assert.IsFalse(left.Equals(rightneg1));
			Assert.IsFalse(left.Equals(rightneg2));
			Assert.IsFalse(left.Equals(rightnull));

			left = new ActionVersion("#add");
			rightpos = new ActionVersion("#add");
			rightneg1 = new ActionVersion("#branch");
			rightneg2 = new Revision(3);
			rightnull = null;

			Assert.IsTrue(left.Equals(rightpos));
			Assert.IsFalse(left.Equals(rightneg1));
			Assert.IsFalse(left.Equals(rightneg2));
			Assert.IsFalse(left.Equals(rightnull));

		}
	}
}
