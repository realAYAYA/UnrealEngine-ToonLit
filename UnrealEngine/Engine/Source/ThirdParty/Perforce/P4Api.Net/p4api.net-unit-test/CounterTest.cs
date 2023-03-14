using Perforce.P4;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using NLog;

namespace p4api.net.unit.test
{
    /// <summary>
    ///This is a test class for CounterTest and is intended
    ///to contain all CounterTest Unit Tests
    ///</summary>
	[TestClass()]
	public class CounterTest
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
        ///A test for Name
        ///</summary>
        [TestMethod()]
		public void NameTest()
		{
			Counter target = new Counter("NAME","1999"); // TODO: Initialize to an appropriate value
			string expected = "NAME";
			string actual;
			target.Name = expected;
			actual = target.Name;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for Value
		///</summary>
		[TestMethod()]
		public void ValueTest()
		{
			Counter target = new Counter("Counter","1999"); // TODO: Initialize to an appropriate value
			string expected = "1999";
			string actual;
			target.Value = expected;
			actual = target.Value;
			Assert.AreEqual(expected, actual);
		}

        /// <summary>
        ///A test for Counter name with special characters
        ///</summary>
        [TestMethod()]
        public void SpecialCharTest()
        {
            Counter target = new Counter("~!@#$%^&*()_+-=", "1999"); // TODO: Initialize to an appropriate value
            string expected = "~!@#$%^&*()_+-=";
            string actual;
            target.Value = expected;
            actual = target.Name;
            Assert.AreEqual(expected, actual);
        }

	}
}
