using Perforce.P4;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using NLog;

namespace p4api.net.unit.test
{
	/// <summary>
	///This is a test class for StringListTest and is intended
	///to contain all StringListTest Unit Tests
	///</summary>
	[TestClass()]
	public class StringEnumTest
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
		///A test for StringEnum Constructor
		///</summary>
		[TestMethod()]
		public void StringEnumConstructorTest()
		{
			Perforce.P4.StringEnum<myEnum> target = new StringEnum<myEnum>(myEnum.Alpha);

			Assert.AreEqual(myEnum.Alpha, (myEnum) target);
		}

		/// <summary>
		///A test for implicit cast from StringEnmum ==> T
		///</summary>
		[TestMethod()]
		public void ImplicitOperatiorTest1()
		{
			StringEnum<myEnum> target = new StringEnum<myEnum>(myEnum.Alpha);

			myEnum actual = target;

			Assert.AreEqual(myEnum.Alpha, actual);
		}

		/// <summary>
		///A test for implicit cast from T ==> StringEnmum
		///</summary>
		[TestMethod()]
		public void ImplicitOperatiorTest2()
		{
			StringEnum<myEnum> target = myEnum.Alpha;

			StringEnum<myEnum> expected = new StringEnum<myEnum>(myEnum.Alpha);

			Assert.AreEqual(expected, target);
		}

		/// <summary>
		///A test for implicit cast from string ==> StringEnmum
		///</summary>
		[TestMethod()]
		public void ImplicitOperatiorTest3()
		{
			StringEnum<myEnum> target = "Alpha";

			StringEnum<myEnum> expected = new StringEnum<myEnum>(myEnum.Alpha);

			Assert.AreEqual(expected, target);
		}

		/// <summary>
		///A test for equality and inequality operators
		///</summary>
		[TestMethod()]
		public void EqualityOperatiorTest()
		{
			StringEnum<myEnum> target = new StringEnum<myEnum>(myEnum.Alpha); ;

			StringEnum<myEnum> expected = new StringEnum<myEnum>(myEnum.Alpha);
			StringEnum<myEnum> notExpected = new StringEnum<myEnum>(myEnum.beta);

			myEnum expected2 = myEnum.Alpha;
			myEnum notExpected2 = myEnum.beta;

			Assert.IsTrue(expected == target);
			Assert.IsTrue(target == expected);

			Assert.IsTrue(notExpected != target);
			Assert.IsTrue(target != notExpected);

			Assert.IsTrue(expected2 == target);
			Assert.IsTrue(target == expected);

			Assert.IsTrue(notExpected2 != target);
			Assert.IsTrue(target != notExpected2);
		}

		/// <summary>
		///A test for ToString
		///</summary>
		[TestMethod()]
		public void ToStringTest()
		{
			StringEnum<myEnum> target = new StringEnum<myEnum>(myEnum.Alpha); ;

			string expected = "Alpha";
			string actual = target.ToString();

			Assert.AreEqual(expected, actual);

			target = new StringEnum<myEnum>(myEnum.beta); ;

			expected = "beta";
			actual = target.ToString();

			Assert.AreEqual(expected, actual);

			target = new StringEnum<myEnum>(myEnum.GAMMA); ;

			expected = "GAMMA";
			actual = target.ToString();

			Assert.AreEqual(expected, actual);

			target = new StringEnum<myEnum>(myEnum.DeltaEpsilon);

			expected = "DeltaEpsilon";
			actual = target.ToString();

			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for ToString(CASE)
		///</summary>
		[TestMethod()]
		public void ToStringTest2()
		{
			// lower
			StringEnum<myEnum> target = new StringEnum<myEnum>(myEnum.Alpha); ;

			string expected = "Alpha".ToLower();
			string actual = target.ToString(StringEnumCase.Lower);

			Assert.AreEqual(expected, actual);

			target = new StringEnum<myEnum>(myEnum.beta); ;

			expected = "beta".ToLower();
			actual = target.ToString(StringEnumCase.Lower);

			Assert.AreEqual(expected, actual);

			target = new StringEnum<myEnum>(myEnum.GAMMA); ;

			expected = "GAMMA".ToLower();
			actual = target.ToString(StringEnumCase.Lower);

			Assert.AreEqual(expected, actual);

			target = new StringEnum<myEnum>(myEnum.DeltaEpsilon);

			expected = "DeltaEpsilon".ToLower();
			actual = target.ToString(StringEnumCase.Lower);

			Assert.AreEqual(expected, actual);

			//upper
			target = new StringEnum<myEnum>(myEnum.Alpha); ;

			expected = "Alpha".ToUpper();
			actual = target.ToString(StringEnumCase.Upper);

			Assert.AreEqual(expected, actual);

			target = new StringEnum<myEnum>(myEnum.beta); ;

			expected = "beta".ToUpper();
			actual = target.ToString(StringEnumCase.Upper);

			Assert.AreEqual(expected, actual);

			target = new StringEnum<myEnum>(myEnum.GAMMA); ;

			expected = "GAMMA".ToUpper();
			actual = target.ToString(StringEnumCase.Upper);

			Assert.AreEqual(expected, actual);

			target = new StringEnum<myEnum>(myEnum.DeltaEpsilon);

			expected = "DeltaEpsilon".ToUpper();
			actual = target.ToString(StringEnumCase.Upper);

			Assert.AreEqual(expected, actual);

			//keep case
			target = new StringEnum<myEnum>(myEnum.Alpha); ;

			expected = "Alpha";
			actual = target.ToString(StringEnumCase.None);

			Assert.AreEqual(expected, actual);

			target = new StringEnum<myEnum>(myEnum.beta); ;

			expected = "beta";
			actual = target.ToString(StringEnumCase.None);

			Assert.AreEqual(expected, actual);

			target = new StringEnum<myEnum>(myEnum.GAMMA); ;

			expected = "GAMMA";
			actual = target.ToString(StringEnumCase.None);

			Assert.AreEqual(expected, actual);

			target = new StringEnum<myEnum>(myEnum.DeltaEpsilon);

			expected = "DeltaEpsilon";
			actual = target.ToString(StringEnumCase.None);

			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for TryParse
		///</summary>
		[TestMethod()]
		public void TryParseTest()
		{
			StringEnum<myEnum> target = null;

			StringEnum<myEnum>.TryParse("Alpha", ref target);

			myEnum expected = myEnum.Alpha;
			myEnum actual = target; // cast to myEnum

			Assert.AreEqual(expected, actual);

			target = null;

			StringEnum<myEnum>.TryParse("beta", ref target);

			expected = myEnum.beta;
			actual = target; // cast to myEnum

			Assert.AreEqual(expected, actual);

			target = null;

			StringEnum<myEnum>.TryParse("GAMMA", ref target);

			expected = myEnum.GAMMA;
			actual = target; // cast to myEnum

			Assert.AreEqual(expected, actual);

			target = null;

			StringEnum<myEnum>.TryParse("DeltaEpsilon", ref target);

			expected = myEnum.DeltaEpsilon;
			actual = target; // cast to myEnum

			Assert.AreEqual(expected, actual);

			// this parse should not work and leave the target as null
			target = null;

			StringEnum<myEnum>.TryParse("badvaluestring", ref target);

			Assert.AreEqual(null, target);
		}

		/// <summary>
		///A test for TryParse with IgnoreCase
		///</summary>
		[TestMethod()]
		public void TryParseTest2()
		{
			StringEnum<myEnum> target = null;

			StringEnum<myEnum>.TryParse("Alpha", true, ref target);

			myEnum expected = myEnum.Alpha;
			myEnum actual = target; // cast to myEnum

			Assert.AreEqual(expected, actual);

			target = null;

			StringEnum<myEnum>.TryParse("ALPhA", true, ref target);

			expected = myEnum.Alpha;
			actual = target; // cast to myEnum

			Assert.AreEqual(expected, actual);

			target = null;

			StringEnum<myEnum>.TryParse("beta", true, ref target);

			expected = myEnum.beta;
			actual = target; // cast to myEnum

			Assert.AreEqual(expected, actual);

			target = null;

			StringEnum<myEnum>.TryParse("BETa", true, ref target);

			expected = myEnum.beta;
			actual = target; // cast to myEnum

			Assert.AreEqual(expected, actual);

			target = null;

			StringEnum<myEnum>.TryParse("GAMMA", true, ref target);

			expected = myEnum.GAMMA;
			actual = target; // cast to myEnum

			Assert.AreEqual(expected, actual);

			target = null;

			StringEnum<myEnum>.TryParse("gammA", true, ref target);

			expected = myEnum.GAMMA;
			actual = target; // cast to myEnum

			Assert.AreEqual(expected, actual);

			target = null;

			StringEnum<myEnum>.TryParse("DeltaEpsilon", true, ref target);

			expected = myEnum.DeltaEpsilon;
			actual = target; // cast to myEnum

			Assert.AreEqual(expected, actual);

			StringEnum<myEnum>.TryParse("DELTAepsilon", true, ref target);

			expected = myEnum.DeltaEpsilon;
			actual = target; // cast to myEnum

			Assert.AreEqual(expected, actual);

			// this parse should not work and leave the target as null
			target = null;

			StringEnum<myEnum>.TryParse("badvaluestring", true, ref target);

			Assert.AreEqual(null, target);
		}
	}
}
