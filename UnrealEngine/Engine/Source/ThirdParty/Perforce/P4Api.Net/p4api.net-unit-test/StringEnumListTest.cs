using Perforce.P4;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System.Collections.Generic;
using NLog;

namespace p4api.net.unit.test
{
    /// <summary>
    ///This is a test class for StringEnumListTest and is intended
    ///to contain all StringEnumListTest Unit Tests
    ///</summary>
	[TestClass()]
	public class StringEnumListTest
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
		///A test for StringEnumList`1 Constructor
		///</summary>
		[TestMethod()]
		public void StringEnumListConstructorTest()
		{
			IList<myEnum> l = new List<myEnum>();

			l.Add(myEnum.Alpha);
			l.Add(myEnum.beta);
			l.Add(myEnum.GAMMA);
			l.Add(myEnum.DeltaEpsilon);

			StringEnumList<myEnum> actual = new StringEnumList<myEnum>(l);
			Assert.AreEqual(4, actual.Count);
		}

		/// <summary>
		///A test for Add
		///</summary>
		[TestMethod()]
		public void AddTest()
		{
			IList<myEnum> target = new List<myEnum>();

			target.Add(myEnum.Alpha);
			target.Add(myEnum.beta);
			target.Add(myEnum.GAMMA);
			target.Add(myEnum.DeltaEpsilon);

			StringEnumList<myEnum> actual = new StringEnumList<myEnum>();

			actual.Add(myEnum.Alpha);
			actual.Add(myEnum.beta);
			actual.Add(myEnum.GAMMA);
			actual.Add(myEnum.DeltaEpsilon);

			Assert.AreEqual(target.Count, actual.Count);
			Assert.AreEqual(target[0], actual[0]);
			Assert.AreEqual(target[1], actual[1]);
			Assert.AreEqual(target[2], actual[2]);
			Assert.AreEqual(target[3], actual[3]);
		}

		/// <summary>
		///A test for Clear
		///</summary>
		[TestMethod()]
		public void ClearTest()
		{
			StringEnumList<myEnum> actual = new StringEnumList<myEnum>();

			actual.Add(myEnum.Alpha);
			actual.Add(myEnum.beta);
			actual.Add(myEnum.GAMMA);
			actual.Add(myEnum.DeltaEpsilon);

			Assert.AreEqual(4, actual.Count);

			actual.Clear();

			Assert.AreEqual(0, actual.Count);
		}

		/// <summary>
		///A test for Contains
		///</summary>
		public void ContainsTestHelper<T>()
		{
		}

		[TestMethod()]
		public void ContainsTest()
		{
			StringEnumList<myEnum> actual = new StringEnumList<myEnum>();

			actual.Add(myEnum.Alpha);
			actual.Add(myEnum.beta);
			actual.Add(myEnum.GAMMA);
			actual.Add(myEnum.DeltaEpsilon);

			Assert.AreEqual(4, actual.Count);

			Assert.IsTrue(actual.Contains(myEnum.Alpha));
			Assert.IsTrue(actual.Contains(myEnum.beta));
			Assert.IsTrue(actual.Contains(myEnum.GAMMA));
			Assert.IsTrue(actual.Contains(myEnum.DeltaEpsilon));
		}

		/// <summary>
		///A test for CopyTo
		///</summary>
		[TestMethod()]
		public void CopyToTest()
		{
			StringEnumList<myEnum> actual = new StringEnumList<myEnum>();

			actual.Add(myEnum.DeltaEpsilon);
			actual.Add(myEnum.DeltaEpsilon);
			actual.Add(myEnum.DeltaEpsilon);
			actual.Add(myEnum.DeltaEpsilon);

			myEnum[] l = new myEnum[] {myEnum.Alpha, myEnum.beta, myEnum.GAMMA, myEnum.DeltaEpsilon};

			actual.CopyTo(l, 0);

			Assert.IsTrue(actual.Contains(myEnum.Alpha));
			Assert.IsTrue(actual.Contains(myEnum.beta));
			Assert.IsTrue(actual.Contains(myEnum.GAMMA));
			Assert.IsTrue(actual.Contains(myEnum.DeltaEpsilon));
		}

		/// <summary>
		///A test for GetEnumerator
		///</summary>
		[TestMethod()]
		public void GetEnumeratorTest()
		{
			StringEnumList<myEnum> l = new StringEnumList<myEnum>();

			l.Add(myEnum.Alpha);
			l.Add(myEnum.beta);
			l.Add(myEnum.GAMMA);
			l.Add(myEnum.DeltaEpsilon);

			IEnumerator<myEnum> actual = l.GetEnumerator();

			actual.MoveNext();
			Assert.AreEqual(myEnum.Alpha, actual.Current);
		}

		/// <summary>
		///A test for IndexOf
		///</summary>
		[TestMethod()]
		public void IndexOfTest()
		{
			StringEnumList<myEnum> l = new StringEnumList<myEnum>();

			l.Add(myEnum.Alpha);
			l.Add(myEnum.beta);
			l.Add(myEnum.GAMMA);

			int actual = l.IndexOf(myEnum.beta);

			Assert.AreEqual(1, actual);

			actual = l.IndexOf(myEnum.DeltaEpsilon);

			Assert.AreEqual(-1, actual);
		}

		/// <summary>
		///A test for Insert
		///</summary>
		[TestMethod()]
		public void InsertTest()
		{
			StringEnumList<myEnum> l = new StringEnumList<myEnum>();

			l.Add(myEnum.Alpha);
			l.Add(myEnum.GAMMA);
			l.Add(myEnum.DeltaEpsilon);

			int actual = l.IndexOf(myEnum.beta);

			Assert.AreEqual(-1, actual);

			l.Insert(1, myEnum.beta);

			actual = l.IndexOf(myEnum.beta);
			Assert.AreEqual(1, actual);
		}

		/// <summary>
		///A test for Remove
		///</summary>
		[TestMethod()]
		public void RemoveTest()
		{
			StringEnumList<myEnum> l = new StringEnumList<myEnum>();

			l.Add(myEnum.Alpha);
			l.Add(myEnum.beta);
			l.Add(myEnum.GAMMA);
			l.Add(myEnum.DeltaEpsilon);

			int actual = l.IndexOf(myEnum.beta);

			Assert.AreEqual(1, actual);

			l.Remove(myEnum.beta);

			actual = l.IndexOf(myEnum.beta);
			Assert.AreEqual(-1, actual);
		}

		/// <summary>
		///A test for RemoveAt
		///</summary>
		[TestMethod()]
		public void RemoveAtTest()
		{
			StringEnumList<myEnum> l = new StringEnumList<myEnum>();

			l.Add(myEnum.Alpha);
			l.Add(myEnum.beta);
			l.Add(myEnum.GAMMA);
			l.Add(myEnum.DeltaEpsilon);

			int actual = l.IndexOf(myEnum.beta);

			Assert.AreEqual(1, actual);

			l.RemoveAt(actual);

			actual = l.IndexOf(myEnum.beta);
			Assert.AreEqual(-1, actual);
		}

		/// <summary>
		///A test for System.Collections.IEnumerable.GetEnumerator
		///</summary>

		[TestMethod()]
		public void GetEnumeratorTest1()
		{
			StringEnumList<myEnum> l = new StringEnumList<myEnum>();

			l.Add(myEnum.Alpha);
			l.Add(myEnum.beta);
			l.Add(myEnum.GAMMA);
			l.Add(myEnum.DeltaEpsilon);

			IEnumerator<myEnum> actual = l.GetEnumerator();

			actual.MoveNext();
			Assert.AreEqual(myEnum.Alpha, actual.Current);
		}

		/// <summary>
		///A test for Count
		///</summary>
		[TestMethod()]
		public void CountTest()
		{
			StringEnumList<myEnum> actual = new StringEnumList<myEnum>();

			actual.Add(myEnum.Alpha);
			actual.Add(myEnum.beta);
			actual.Add(myEnum.GAMMA);
			actual.Add(myEnum.DeltaEpsilon);

			Assert.AreEqual(4, actual.Count);
		}

		/// <summary>
		///A test for IsReadOnly
		///</summary>
		public void IsReadOnlyTestHelper<T>()
		{
		}

		[TestMethod()]
		public void IsReadOnlyTest()
		{
			StringEnumList<myEnum> actual = new StringEnumList<myEnum>();

			actual.Add(myEnum.Alpha);
			actual.Add(myEnum.beta);
			actual.Add(myEnum.GAMMA);
			actual.Add(myEnum.DeltaEpsilon);

			Assert.IsFalse(actual.IsReadOnly);
		}

		/// <summary>
		///A test for Item
		///</summary>
		[TestMethod()]
		public void ItemTest()
		{
			StringEnumList<myEnum> actual = new StringEnumList<myEnum>();

			actual.Add(myEnum.Alpha);
			actual.Add(myEnum.beta);
			actual.Add(myEnum.GAMMA);
			actual.Add(myEnum.DeltaEpsilon);

			Assert.AreEqual(myEnum.beta, actual[1]);
		}
	}
}
