using Perforce.P4;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System.Collections.Generic;
using NLog;

namespace p4api.net.unit.test
{
    /// <summary>
    ///This is a test class for StringEnumListEnumeratorTest and is intended
    ///to contain all StringEnumListEnumeratorTest Unit Tests
    ///</summary>
	[TestClass()]
	public class StringEnumListEnumeratorTest
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

        enum myEnum { Alpha, beta, GAMMA, DeltaEpsilon };

		/// <summary>
		///A test for StringEnumListEnumerator`1 Constructor
		///</summary>
		[TestMethod()]
		public void StringEnumListEnumeratorConstructorTest()
		{
			IList<StringEnum<myEnum>> l = new List<StringEnum<myEnum>>();

			l.Add(myEnum.Alpha);
			l.Add(myEnum.beta);
			l.Add(myEnum.GAMMA);
			l.Add(myEnum.DeltaEpsilon);

			StringEnumListEnumerator<myEnum> actual = new StringEnumListEnumerator<myEnum>(l);
			Assert.IsNotNull(actual);
		}

		/// <summary>
		///A test for MoveNext
		///</summary>
		[TestMethod()]
		public void MoveNextTest()
		{
			IList<StringEnum<myEnum>> l = new List<StringEnum<myEnum>>();

			l.Add(myEnum.Alpha);
			l.Add(myEnum.beta);
			l.Add(myEnum.GAMMA);
			l.Add(myEnum.DeltaEpsilon);

			StringEnumListEnumerator<myEnum> actual = new StringEnumListEnumerator<myEnum>(l);

			actual.MoveNext();
			Assert.AreEqual(myEnum.Alpha, actual.Current);
			actual.MoveNext();
			Assert.AreEqual(myEnum.beta, actual.Current);
			actual.MoveNext();
			Assert.AreEqual(myEnum.GAMMA, actual.Current);
		}

		/// <summary>
		///A test for Reset
		///</summary>
		[TestMethod()]
		public void ResetTest()
		{
			IList<StringEnum<myEnum>> l = new List<StringEnum<myEnum>>();

			l.Add(myEnum.Alpha);
			l.Add(myEnum.beta);
			l.Add(myEnum.GAMMA);
			l.Add(myEnum.DeltaEpsilon);

			StringEnumListEnumerator<myEnum> actual = new StringEnumListEnumerator<myEnum>(l);

			actual.MoveNext();
			Assert.AreEqual(myEnum.Alpha, actual.Current);
			actual.MoveNext();
			Assert.AreEqual(myEnum.beta, actual.Current);
			actual.MoveNext();
			Assert.AreEqual(myEnum.GAMMA, actual.Current);

			actual.Reset();
			actual.MoveNext();
			Assert.AreEqual(myEnum.Alpha, actual.Current);
		}


		/// <summary>
		///A test for Current
		///</summary>
		[TestMethod()]
		public void CurrentTest1()
		{
			//Tested by MoveNextTest()
			MoveNextTest();
		}
	}
}
