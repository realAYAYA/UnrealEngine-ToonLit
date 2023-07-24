using Perforce.P4;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using NLog;

namespace p4api.net.unit.test
{
    /// <summary>
    ///This is a test class for P4MapApiTest and is intended
    ///to contain all P4MapApiTest Unit Tests
    ///</summary>
    [TestClass()]
    public class P4MapApiTest
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
        ///A test for Insert
        ///</summary>
        [TestMethod()]
        public void InsertTest()
        {
            bool unicode = false;
            for (int i = 0; i < 2; i++)
            {
                using (P4MapApi target = new P4MapApi(unicode))
                {
                    string left1 = "//depot/main/...";
                    string right1 = "//XP1_usr/depot/main/...";
                    P4MapApi.Type type1 = P4MapApi.Type.Include;
                    target.Insert(left1, right1, type1);

                    string left2 = "//depot/main/www/...";
                    string right2 = "//XP1_usr/depot/main/www/...";
                    P4MapApi.Type type2 = P4MapApi.Type.Exclude;
                    target.Insert(left2, right2, type2);


                    string left3 = "//depot/dev/script/...";
                    string right3 = "//XP1_usr/depot/dev/script/...";
                    P4MapApi.Type type3 = P4MapApi.Type.Include;
                    target.Insert(left3, right3, type3);


                    string left4 = "//depot/main/www/...";
                    string right4 = "//XP1_usr/depot/live/www/*.html";
                    P4MapApi.Type type4 = P4MapApi.Type.Include;
                    target.Insert(left4, right4, type4);

                    int tCount = target.Count;
                    Assert.AreEqual(4, tCount);

                    // no errors so passed
                }
            }
        }

        /// <summary>
        ///A test for Count
        ///</summary>
        [TestMethod()]
        public void CountTest()
        {
            bool unicode = false;
            for (int i = 0; i < 2; i++)
            {
                using (P4MapApi target = new P4MapApi(unicode))
                {
                    string left1 = "//depot/main/...";
                    string right1 = "//XP1_usr/depot/main/...";
                    P4MapApi.Type type1 = P4MapApi.Type.Include;
                    target.Insert(left1, right1, type1);

                    string left2 = "//depot/main/www/...";
                    string right2 = "//XP1_usr/depot/main/www/...";
                    P4MapApi.Type type2 = P4MapApi.Type.Exclude;
                    target.Insert(left2, right2, type2);


                    string left3 = "//depot/dev/script/...";
                    string right3 = "//XP1_usr/depot/dev/script/...";
                    P4MapApi.Type type3 = P4MapApi.Type.Include;
                    target.Insert(left3, right3, type3);


                    string left4 = "//depot/main/www/...";
                    string right4 = "//XP1_usr/depot/live/www/*.html";
                    P4MapApi.Type type4 = P4MapApi.Type.Include;
                    target.Insert(left4, right4, type4);

                    int tCount = target.Count;
                    Assert.AreEqual(4, tCount);

                    unicode = !unicode;
                }
            }
        }

        /// <summary>
        ///A test for Translate
        ///</summary>
        [TestMethod()]
        public void TranslateTest()
        {
            bool unicode = false;
            for (int i = 0; i < 2; i++)
            {
                using (P4MapApi target = new P4MapApi(unicode))
                {
                    String left1 = "//depot/...";
                    String right1 = "//ws/...";
                    P4MapApi.Type type1 = P4MapApi.Type.Include;
                    target.Insert(left1, right1, type1);

                    int tCount = target.Count;
                    Assert.AreEqual(1, tCount);

                    String actual = target.Translate("//ws/foo.txt", P4MapApi.Direction.RightLeft);
                    Assert.AreEqual(actual, "//depot/foo.txt");

                    actual = target.Translate("//depot/foo.txt", P4MapApi.Direction.LeftRight);
                    Assert.AreEqual(actual, "//ws/foo.txt");

                    actual = target.Translate("//usr/bar.txt", P4MapApi.Direction.LeftRight);
                    Assert.AreEqual(actual, null);

                    unicode = !unicode;
                }
            }
        }

        /// <summary>
        ///A test for Join
        ///</summary>
        [TestMethod()]
        public void JoinTest()
        {
            bool unicode = false;
            for (int i = 0; i < 2; i++)
            {
                using (P4MapApi target1 = new P4MapApi(unicode))
                {
                    String left1 = "//depot/...";
                    String right1 = "//ws/...";
                    P4MapApi.Type type1 = P4MapApi.Type.Include;
                    target1.Insert(left1, right1, type1);

                    int tCount1 = target1.Count;
                    Assert.AreEqual(1, tCount1);

                    using (P4MapApi target2 = new P4MapApi(unicode))
                    {
                        String left2 = "//ws/...";
                        String right2 = "//ace/...";
                        P4MapApi.Type type2 = P4MapApi.Type.Include;
                        target2.Insert(left2, right2, type2);

                        int tCount2 = target2.Count;
                        Assert.AreEqual(1, tCount2);

                        using (P4MapApi actual = P4MapApi.Join(target1, P4MapApi.Direction.LeftRight, target2, P4MapApi.Direction.LeftRight))
                        {
                            int tCount3 = actual.Count;
                            Assert.AreEqual(1, tCount3);

                            string translated = actual.Translate("//depot/foo.txt", P4MapApi.Direction.LeftRight);
                            Assert.AreEqual(translated, "//ace/foo.txt");

                            unicode = !unicode;
                        }
                    }
                }
            }
        }

        /// <summary>
        ///A test for Join
        ///</summary>
        [TestMethod()]
        public void JoinTest1()
        {
            bool unicode = false;
            for (int i = 0; i < 2; i++)
            {
                using (P4MapApi target1 = new P4MapApi(unicode))
                {
                    String lr = "//ws/directory/...";
                    P4MapApi.Type type1 = P4MapApi.Type.Include;
                    target1.Insert(lr, type1);

                    int tCount1 = target1.Count;
                    Assert.AreEqual(1, tCount1);

                    using (P4MapApi target2 = new P4MapApi(unicode))
                    {
                        String left2 = "//ws/...";
                        String right2 = "//depot/...";
                        P4MapApi.Type type2 = P4MapApi.Type.Include;
                        target2.Insert(left2, right2, type2);

                        int tCount2 = target2.Count;
                        Assert.AreEqual(1, tCount2);

                        using (P4MapApi actual = P4MapApi.Join(target1, target2))
                        {
                            int tCount3 = actual.Count;
                            Assert.AreEqual(1, tCount3);

                            string translated = actual.Translate("//ws/directory/foo.txt", P4MapApi.Direction.LeftRight);
                            Assert.AreEqual(translated, "//depot/directory/foo.txt");

                            unicode = !unicode;
                        }
                    }
                }
            }
        }

        /// <summary>
        ///A test for Insert
        ///</summary>
        [TestMethod()]
        public void InsertTest1()
        {
            bool unicode = false;
            for (int i = 0; i < 2; i++)
            {
                using (P4Server pserver = new P4Server(unicode))
                {
                    using (P4MapApi target = new P4MapApi(pserver))
                    {
                        string lr1 = "Both 1";
                        P4MapApi.Type type1 = P4MapApi.Type.Include;
                        target.Insert(lr1, type1);

                        string lr2 = "Both 2";
                        P4MapApi.Type type2 = P4MapApi.Type.Include;
                        target.Insert(lr2, type2);

                        int tCount = target.Count;
                        Assert.AreEqual(2, tCount);

                        // no errors so passed
                    }
                }
            }
        }

        /// <summary>
        ///A test for GetType
        ///</summary>
        [TestMethod()]
        public void GetTypeTest()
        {
            bool unicode = false;
            for (int i = 0; i < 2; i++)
            {
                using (P4MapApi target = new P4MapApi(unicode))
                {
                    string left1 = "//depot/main/...";
                    string right1 = "//XP1_usr/depot/main/...";
                    P4MapApi.Type type1 = P4MapApi.Type.Include;
                    target.Insert(left1, right1, type1);

                    string left2 = "//depot/main/www/...";
                    string right2 = "//XP1_usr/depot/main/www/...";
                    P4MapApi.Type type2 = P4MapApi.Type.Exclude;
                    target.Insert(left2, right2, type2);


                    string left3 = "//depot/dev/script/...";
                    string right3 = "//XP1_usr/depot/dev/script/...";
                    P4MapApi.Type type3 = P4MapApi.Type.Include;
                    target.Insert(left3, right3, type3);


                    string left4 = "//depot/main/www/...";
                    string right4 = "//XP1_usr/depot/live/www/*.html";
                    P4MapApi.Type type4 = P4MapApi.Type.Include;
                    target.Insert(left4, right4, type4);

                    int tCount = target.Count;
                    Assert.AreEqual(4, tCount);

                    P4MapApi.Type actual = target.GetType(1);
                    Assert.AreEqual(type2, actual);

                    actual = target.GetType(0);
                    Assert.AreEqual(type1, actual);

                    unicode = !unicode;
                }
            }
        }

        /// <summary>
        ///A test for GetRight
        ///</summary>
        [TestMethod()]
        public void GetRightTest()
        {
            bool unicode = false;
            for (int i = 0; i < 2; i++)
            {
                using (P4MapApi target = new P4MapApi(unicode))
                {
                    string left1 = "//depot/main/...";
                    string right1 = "//XP1_usr/depot/main/...";
                    P4MapApi.Type type1 = P4MapApi.Type.Include;
                    target.Insert(left1, right1, type1);

                    string left2 = "//depot/main/www/...";
                    string right2 = "//XP1_usr/depot/main/www/...";
                    P4MapApi.Type type2 = P4MapApi.Type.Exclude;
                    target.Insert(left2, right2, type2);


                    string left3 = "//depot/dev/script/...";
                    string right3 = "//XP1_usr/depot/dev/script/...";
                    P4MapApi.Type type3 = P4MapApi.Type.Include;
                    target.Insert(left3, right3, type3);


                    string left4 = "//depot/main/www/...";
                    string right4 = "//XP1_usr/depot/live/www/*.html";
                    P4MapApi.Type type4 = P4MapApi.Type.Include;
                    target.Insert(left4, right4, type4);

                    int tCount = target.Count;
                    Assert.AreEqual(4, tCount);

                    String actual = target.GetRight(1);
                    Assert.AreEqual(right2, actual);

                    actual = target.GetRight(0);
                    Assert.AreEqual(right1, actual);

                    unicode = !unicode;
                }
            }
        }

        /// <summary>
        ///A test for GetLeft
        ///</summary>
        [TestMethod()]
        public void GetLeftTest()
        {
            bool unicode = false;
            for (int i = 0; i < 2; i++)
            {
                using (P4MapApi target = new P4MapApi(unicode))
                {
                    string left1 = "//depot/main/...";
                    string right1 = "//XP1_usr/depot/main/...";
                    P4MapApi.Type type1 = P4MapApi.Type.Include;
                    target.Insert(left1, right1, type1);

                    string left2 = "//depot/main/www/...";
                    string right2 = "//XP1_usr/depot/main/www/...";
                    P4MapApi.Type type2 = P4MapApi.Type.Exclude;
                    target.Insert(left2, right2, type2);


                    string left3 = "//depot/dev/script/...";
                    string right3 = "//XP1_usr/depot/dev/script/...";
                    P4MapApi.Type type3 = P4MapApi.Type.Include;
                    target.Insert(left3, right3, type3);


                    string left4 = "//depot/main/www/...";
                    string right4 = "//XP1_usr/depot/live/www/*.html";
                    P4MapApi.Type type4 = P4MapApi.Type.Include;
                    target.Insert(left4, right4, type4);

                    int tCount = target.Count;
                    Assert.AreEqual(4, tCount);

                    String actual = target.GetLeft(1);
                    Assert.AreEqual(left2, actual);

                    actual = target.GetLeft(0);
                    Assert.AreEqual(left1, actual);

                    unicode = !unicode;
                }
            }
        }

        /// <summary>
        ///A test for Clear
        ///</summary>
        [TestMethod()]
        public void ClearTest()
        {
            bool unicode = false;
            for (int i = 0; i < 2; i++)
            {
                using (P4MapApi target = new P4MapApi(unicode))
                {
                    string left1 = "//depot/main/...";
                    string right1 = "//XP1_usr/depot/main/...";
                    P4MapApi.Type type1 = P4MapApi.Type.Include;
                    target.Insert(left1, right1, type1);

                    string left2 = "//depot/main/www/...";
                    string right2 = "//XP1_usr/depot/main/www/...";
                    P4MapApi.Type type2 = P4MapApi.Type.Exclude;
                    target.Insert(left2, right2, type2);


                    string left3 = "//depot/dev/script/...";
                    string right3 = "//XP1_usr/depot/dev/script/...";
                    P4MapApi.Type type3 = P4MapApi.Type.Include;
                    target.Insert(left3, right3, type3);


                    string left4 = "//depot/main/www/...";
                    string right4 = "//XP1_usr/depot/live/www/*.html";
                    P4MapApi.Type type4 = P4MapApi.Type.Include;
                    target.Insert(left4, right4, type4);

                    int actual = target.Count;
                    Assert.AreEqual(4, actual);

                    target.Clear();

                    actual = target.Count;
                    Assert.AreEqual(0, actual);

                    unicode = !unicode;
                }
            }
        }
    }
}
