using Perforce.P4;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using NLog;

namespace p4api.net.unit.test
{
    /// <summary>
    ///This is a test class for StringListTest and is intended
    ///to contain all StringListTest Unit Tests
    ///</summary>
    [TestClass()]
    public class StringListTest
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
        ///A test for StringList Constructor
        ///</summary>
        [TestMethod()]
        public void StringListConstructorTest()
        {
            String[] l = new String[] {"1","02", "003"};
            StringList target = new StringList(l);
            Assert.AreEqual(l.Length, target.Count);
            Assert.AreEqual(l[0], target[0]);
            Assert.AreEqual(l[1], target[1]);
            Assert.AreEqual(l[2], target[2]);
        }

        /// <summary>
        ///A test for StringList Constructor
        ///</summary>
        [TestMethod()]
        public void StringListConstructorTest1()
        {
            int capacity = 5; 
            StringList target = new StringList(capacity);
            Assert.IsTrue(capacity <= target.Capacity);
            Assert.AreEqual(null, target[capacity-1]);
        }

        /// <summary>
        ///A test for Copy
        ///</summary>
        [TestMethod()]
        public void CopyTest()
        {
            StringList src = new StringList(new String[] { "1", "02", "003" });
            StringList target = new StringList();

            //copy to an empty list
            int destIdx = 0;
            int cnt = 3;
            target.Copy(src, destIdx, cnt);
            Assert.AreEqual(src[1], target[destIdx + 1]);

            // add after the end of a list
            destIdx = 5;
            cnt = 3;
            target.Copy(src, destIdx, cnt);
            Assert.AreEqual(src[1], target[destIdx + 1]);

            //overwrite existing elements
            src = new StringList(new String[] { "1a", "02a", "003a" });
            destIdx = 0;
            cnt = 3;
            target.Copy(src, destIdx, cnt);
            Assert.AreEqual(src[1], target[destIdx + 1]);
        }

        /// <summary>
        ///A test for Equals
        ///</summary>
        [TestMethod()]
        public void EqualsTest()
        {
            StringList target = new StringList(new String[] { "1", "02", "003" });
            object obj = new StringList(new String[] { "1", "02", "003" });
            bool expected = true;
            bool actual;
            actual = target.Equals(obj);
            Assert.AreEqual(expected, actual);

            obj = "Not a list";
            expected = false;
            actual = target.Equals(obj);
            Assert.AreEqual(expected, actual);

            obj = null;
            expected = false;
            actual = target.Equals(obj);
            Assert.AreEqual(expected, actual);

            obj = new StringList(new String[] { "1", "003", "02" });
            expected = false;
            actual = target.Equals(obj);
            Assert.AreEqual(expected, actual);

            obj = new StringList(new String[] { "1", "02", "003", "0004" });
            expected = false;
            actual = target.Equals(obj);
            Assert.AreEqual(expected, actual);
        }

        /// <summary>
        ///A test for IsNullOrEmpy
        ///</summary>
        [TestMethod()]
        public void IsNullOrEmpyTest()
        {
            StringList target = null;
            bool expected = true;
            bool actual;
            actual = StringList.IsNullOrEmpy(target);
            Assert.AreEqual(expected, actual);

            target = new StringList();
            expected = true;
            actual = StringList.IsNullOrEmpy(target);
            Assert.AreEqual(expected, actual);

            target = new StringList(new String[] { "1", "003", "02" });
            expected = false;
            actual = StringList.IsNullOrEmpy(target);
            Assert.AreEqual(expected, actual);
        }

        /// <summary>
        ///A test for ToString
        ///</summary>
        [TestMethod()]
        public void ToStringTest()
        {
            StringList target = new StringList(new String[] { "1", "003", "02" });
            string expected = "1/r/n003/r/n02";
            string actual;
            actual = target.ToString();
            Assert.AreEqual(expected, actual);
        }

        /// <summary>
        ///A test for op_Addition
        ///</summary>
        [TestMethod()]
        public void op_AdditionTest()
        {
            StringList l = new StringList(new String[] { "1", "02", "003" });
            StringList r = new StringList(new String[] { "0004", "00005", "000006" });
            StringList expected = new StringList(new String[] { "1", "02", "003", "0004", "00005", "000006" });
            StringList actual;
            actual = (l + r);
            Assert.AreEqual(expected, actual);
        }

        /// <summary>
        ///A test for op_Equality
        ///</summary>
        [TestMethod()]
        public void op_EqualityTest()
        {
            StringList l1 = new StringList(new String[] { "1", "02", "003" });
            StringList l2 = new StringList(new String[] { "1", "02", "003" });
            bool expected = true;
            bool actual;
            actual = (l1 == l2);
            Assert.AreEqual(expected, actual);

            l2 = new StringList(new String[] { "1", "003", "02" });
            expected = false;
            actual = (l1 == l2);
            Assert.AreEqual(expected, actual);

            l2 = new StringList(new String[] { "1", "02", "003", "0004" });
            expected = false;
            actual = (l1 == l2);
            Assert.AreEqual(expected, actual);
        }

        /// <summary>
        ///A test for op_Implicit
        ///</summary>
        [TestMethod()]
        public void op_ImplicitTest()
        {
            string[] l = new String[] { "1", "02", "003" };
            StringList actual;
            actual = l;
            Assert.IsTrue(l.Length <= actual.Capacity);
            Assert.AreEqual(l[0], actual[0]);
            Assert.AreEqual(l[1], actual[1]);
            Assert.AreEqual(l[2], actual[2]);
        }

        /// <summary>
        ///A test for op_Implicit
        ///</summary>
        [TestMethod()]
        public void op_ImplicitTest1()
        {
            StringList l = new StringList(new String[] { "1", "02", "003" });
            string[] expected = new String[] { "1", "02", "003" };
            string[] actual;
            actual = l;
            Assert.AreEqual(expected.Length, actual.Length);
            Assert.AreEqual(expected[0], actual[0]);
            Assert.AreEqual(expected[1], actual[1]);
            Assert.AreEqual(expected[2], actual[2]);
        }

        /// <summary>
        ///A test for op_Inequality
        ///</summary>
        [TestMethod()]
        public void op_InequalityTest()
        {
            StringList l1 = new StringList(new String[] { "1", "02", "003" });
            StringList l2 = new StringList(new String[] { "1", "02", "003" });
            bool expected = false;
            bool actual;
            actual = (l1 != l2);
            Assert.AreEqual(expected, actual);

            l2 = new StringList(new String[] { "1", "003", "02" });
            expected = true;
            actual = (l1 != l2);
            Assert.AreEqual(expected, actual);

            l2 = new StringList(new String[] { "1", "02", "003", "0004" });
            expected = true;
            actual = (l1 != l2);
            Assert.AreEqual(expected, actual);
        }
    }
}
