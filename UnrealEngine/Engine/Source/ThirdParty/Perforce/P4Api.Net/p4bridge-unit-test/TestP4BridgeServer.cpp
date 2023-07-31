#include "stdafx.h"
#include "UnitTestFrameWork.h"
#include "TestP4BridgeServer.h"
#include "TextEncoder.h"

#include "../p4bridge/P4BridgeClient.h"
#include "../p4bridge/P4BridgeServer.h"
#include "../p4bridge/P4BridgeEnviro.h"
#include "../p4bridge/P4Connection.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <io.h>
#include <conio.h>
#include <ios>
#include <string>
#include <sstream>
#include <fstream>

// Does not return the right value, but is available in VS2010, and does most of what we want
#define snprintf(buf,len, format,...) _snprintf_s(buf, len,len, format, __VA_ARGS__)

CREATE_TEST_SUITE(TestP4BridgeServer)

TestP4BridgeServer::TestP4BridgeServer(void)
{
	UnitTestSuite::RegisterTest(TestGetConfig, "TestGetConfig");
	UnitTestSuite::RegisterTest(TestGetSet, "TestGetSet");
	UnitTestSuite::RegisterTest(ServerConnectionTest, "ServerConnectionTest");
    UnitTestSuite::RegisterTest(TestUnicodeClientToNonUnicodeServer, "TestUnicodeClientToNonUnicodeServer");
    UnitTestSuite::RegisterTest(TestUnicodeUserName, "TestUnicodeUserName");
    UnitTestSuite::RegisterTest(TestUntaggedCommand, "TestUntaggedCommand");
    UnitTestSuite::RegisterTest(TestTaggedCommand, "TestTaggedCommand");
    UnitTestSuite::RegisterTest(TestTextOutCommand, "TestTextOutCommand");
    UnitTestSuite::RegisterTest(TestBinaryOutCommand, "TestBinaryOutCommand");
    UnitTestSuite::RegisterTest(TestErrorOutCommand, "TestErrorOutCommand");
    UnitTestSuite::RegisterTest(TestEnviro, "TestEnviro");
	UnitTestSuite::RegisterTest(TestConnectSetClient, "TestConnectSetClient");
	UnitTestSuite::RegisterTest(TestIsIgnored, "TestIsIgnored");
    UnitTestSuite::RegisterTest(TestSetVars, "TestSetVars");
    UnitTestSuite::RegisterTest(TestParallelSync, "TestParallelSync"); 
    UnitTestSuite::RegisterTest(TestDefaultProgramNameAndVersion, "TestDefaultProgramNameAndVersion");
    UnitTestSuite::RegisterTest(TestGetTicketFile, "TestGetTicketFile");
	UnitTestSuite::RegisterTest(TestSetTicketFile, "TestSetTicketFile");
	UnitTestSuite::RegisterTest(TestParallelSyncCallback, "TestParallelSyncCallback");
	UnitTestSuite::RegisterTest(TestSetProtocol, "TestSetProtocol");
}


TestP4BridgeServer::~TestP4BridgeServer(void)
{
}

char unitTestDir[MAX_PATH];
char unitTestZip[MAX_PATH];
char * TestDir = "c:\\MyTestDir";
char * TestZip = "c:\\MyTestDir\\a.exe";
char * rcp_cmd = "p4d -r C:/MyTestDir -jr checkpoint.1";
char * udb_cmd = "p4d -r C:/MyTestDir -xu";
char * p4d_cmd = "p4d -p6666 -IdUnitTestServer -rC:/MyTestDir -Llog";
char * TestLog = "c:\\MyTestDir\\log";
char * testCfgFile = "C:\\MyTestDir\\admin_space\\myP4Config.txt";
char * testTicketFile = "C:\\MyTestDir\\admin_space\\p4tickets.txt";
char * testCfgDir = "C:\\MyTestDir\\admin_space";
char * testIgnoreFile = "C:\\MyTestDir\\admin_space\\myP4Ignore.txt";
char * testIgnoredFile1 = "C:\\MyTestDir\\admin_space\\foofoofoo.foo";
char * testIgnoredFile2 = "C:\\MyTestDir\\admin_space\\moomoomoo.moo";
char * testProgramName = "BridgeUnitTests";
char * testProgramVer = "1.2.3.4.A.b.C";

void * pi = NULL;
P4BridgeServer * ps = NULL;

string oldConfig;
string oldIgnore;

bool TestP4BridgeServer::Setup()
{
    // remove the test directory if it exists
    UnitTestSuite::rmDir( TestDir ) ;

    GetCurrentDirectory(sizeof(unitTestDir), unitTestDir);

    strcpy( unitTestZip, unitTestDir);
    strcat( unitTestZip, "\\a.exe");

    if (!CreateDirectory( TestDir, NULL))
    {
		DWORD err = GetLastError();
		printf("GetLastError returns %ld\n",err);

	    return false;
    }
		
    if (!CopyFile(unitTestZip, TestZip, false)) 
		return false;

    if (!SetCurrentDirectory(TestDir)) 
		return false;

    pi = UnitTestSuite::RunProgram("a", TestDir, true, true);
    if (!pi) 
    {
        SetCurrentDirectory(unitTestDir);
        return false;
    }

    delete pi;

    pi = UnitTestSuite::RunProgram(rcp_cmd, TestDir, true, true);
    if (!pi) 
    {
        SetCurrentDirectory(unitTestDir);
        return false;
    }

    delete pi;

    pi = UnitTestSuite::RunProgram(udb_cmd, TestDir, true, true);
    if (!pi) 
    {
        SetCurrentDirectory(unitTestDir);
        return false;
    }

    delete pi;

    pi = UnitTestSuite::RunProgram(p4d_cmd, TestDir, false, false);
    if (!pi) 
    {
        SetCurrentDirectory(unitTestDir);
        return false;
    }

//    _getch();

    return true;
}

int TestP4BridgeServer::LogCallback(int level, const char *file, int line, const char *msg)
{
	printf("LOG: %d %s:%d %s\n", level, file, line , msg);
	return 1;
}

bool TestP4BridgeServer::TearDown(char* testName)
{
	UnitTestSuite::rmDir( testCfgFile ) ;
    UnitTestSuite::rmDir( testIgnoreFile ) ;
	if (!oldConfig.empty())
	{
		P4BridgeServer::Set("P4CONFIG", oldConfig.c_str());	
		oldConfig.clear();
	}
	if (!oldIgnore.empty())
	{
		P4BridgeServer::Set("P4IGNORE", oldIgnore.c_str());	
		oldIgnore.clear();
	}
    if (pi)
        UnitTestSuite::EndProcess( (LPPROCESS_INFORMATION) pi );
	if (ps)
    {
		delete ps;
		ps = NULL;

		//p4base::DumpMemoryState("After deleting server");
	}
    SetCurrentDirectory(unitTestDir);

    UnitTestSuite::rmDir( TestDir ) ;

	p4base::PrintMemoryState(testName);

    return true;
}

bool TestP4BridgeServer::ServerConnectionTest()
{
    P4ClientError* connectionError = NULL;
    // create a new server
    ps = new P4BridgeServer("localhost:6666", "admin", "", "");
    ASSERT_NOT_NULL(ps);
	//ps->SetLogCallFn((LogCallbackFn *) &TestP4BridgeServer::LogCallback);

    // connect and see if the api returned an error. 
    if( !ps->connected( &connectionError ) )
    {
        char buff[256];
		snprintf(buff, 255, "Connection error: %s", connectionError->Message);
        // Abort if the connect did not succeed
        ASSERT_FAIL(buff);
    }

    return true;
}

bool TestP4BridgeServer::TestUnicodeClientToNonUnicodeServer()
{
    P4ClientError* connectionError = NULL;
    // create a new server
    ps = new P4BridgeServer("localhost:6666", "admin", "", "admin_space");
    ASSERT_NOT_NULL(ps);

    // connect and see if the api returned an error. 
    if( !ps->connected( &connectionError ) )
    {
        char buff[256];
		snprintf(buff, 255, "Connection error: %s", connectionError->Message);
        // Abort if the connect did not succeed
        ASSERT_FAIL(buff);
    }

    ASSERT_NOT_EQUAL(ps->unicodeServer(), 1);
    ps->set_charset("utf8", "utf16le");

    char* params[1];
    params[0] = "//depot/mycode/*";

    ASSERT_FALSE(ps->run_command("files", 3456, 0, params, 1))

    P4ClientError * out = ps->get_ui(3456)->GetErrorResults();

    ASSERT_STRING_STARTS_WITH(out->Message.c_str(), "Unicode clients require a unicode enabled server.")

    return true;
}

bool TestP4BridgeServer::TestUnicodeUserName()
{

    P4ClientError* connectionError = NULL;
    // create a new server
    //Aleksey (Alexei) in Cyrillic = "\xD0\x90\xD0\xbb\xD0\xB5\xD0\xBA\xD1\x81\xD0\xB5\xD0\xB9\0" IN utf-8
    ps = new P4BridgeServer("localhost:6666", "\xD0\x90\xD0\xBB\xD0\xB5\xD0\xBA\xD1\x81\xD0\xB5\xD0\xB9\0", "pass", "\xD0\x90\xD0\xbb\xD0\xB5\xD0\xBA\xD1\x81\xD0\xB5\xD0\xB9\0");
    ASSERT_NOT_NULL(ps);

    // connect and see if the api returned an error. 
    if( !ps->connected( &connectionError ) )
    {
        char buff[256];
		snprintf(buff, 255, "Connection error: %s", connectionError->Message);
        // Abort if the connect did not succeed
        ASSERT_FAIL(buff);
    }

    ASSERT_FALSE(ps->unicodeServer());
    ps->set_charset("utf8", "utf16le");
	// ps->set_charset("none", "none");                                  // This disables client P4CHARSET

    char* params[1];
    params[0] = "//depot/mycode/*";

    ASSERT_FALSE(ps->run_command("files", 7, 0, params, 1))

    P4ClientError * out = ps->get_ui(7)->GetErrorResults();

    ASSERT_STRING_STARTS_WITH(out->Message.c_str(), "Unicode clients require a unicode enabled server.")

    return true;
}

bool TestP4BridgeServer::TestUntaggedCommand()
{
    P4ClientError* connectionError = NULL;
    // create a new server
    ps = new P4BridgeServer("localhost:6666", "admin", "", "admin_space");
    ASSERT_NOT_NULL(ps);

    // connect and see if the api returned an error. 
    if( !ps->connected( &connectionError ) )
    {
        char buff[256];
		snprintf(buff, 255, "Connection error: %s", *connectionError);
        // Abort if the connect did not succeed
        ASSERT_FAIL(buff);
    }
    char* params[1];
    params[0] = "//depot/mycode/*";

    ASSERT_INT_TRUE(ps->run_command("files", 7, 0, params, 1))

    P4ClientInfoMsg * out = ps->get_ui(7)->GetInfoResults();

    ASSERT_STRING_STARTS_WITH(out->Message.c_str(), "//depot/MyCode/")

    return true;
}

bool TestP4BridgeServer::TestTaggedCommand()
{
    P4ClientError* connectionError = NULL;
    // create a new server
    ps = new P4BridgeServer("localhost:6666", "admin", "", "admin_space");
    ASSERT_NOT_NULL(ps);

    // connect and see if the api returned an error. 
    if( !ps->connected( &connectionError ) )
    {
        char buff[256];
		snprintf(buff, 255, "Connection error: %s", connectionError->Message);
        // Abort if the connect did not succeed
        ASSERT_FAIL(buff);
    }
    char* params[1];
    params[0] = "//depot/mycode/*";

    ASSERT_INT_TRUE(ps->run_command("files", 7, 1, params, 1))

    StrDictListIterator * out = ps->get_ui(7)->GetTaggedOutput();

    ASSERT_NOT_NULL(out);

    int itemCnt = 0;
    while (StrDictList * pItem = out->GetNextItem())
    {
        int entryCnt = 0;

        while (KeyValuePair * pEntry = out->GetNextEntry())
        {
			ASSERT_TRUE(pEntry->key != "func");
            if ((itemCnt == 0) && (strcmp(pEntry->key.c_str(), "depotFile") == 0))
                ASSERT_STRING_STARTS_WITH(pEntry->value.c_str(), "//depot/MyCode/")
            if ((itemCnt == 1) && (strcmp(pEntry->key.c_str(), "depotFile") == 0))
                ASSERT_STRING_STARTS_WITH(pEntry->value.c_str(), "//depot/MyCode/")
            entryCnt++;
        }
        ASSERT_NOT_EQUAL(entryCnt, 0);
        itemCnt++;
    }
    ASSERT_EQUAL(itemCnt, 3);

	delete out;

    return true;
}

bool TestP4BridgeServer::TestTextOutCommand()
{
    P4ClientError* connectionError = NULL;
    // create a new server
    ps = new P4BridgeServer("localhost:6666", "admin", "", "admin_space");
    ASSERT_NOT_NULL(ps);

    // connect and see if the api returned an error. 
    if( !ps->connected( &connectionError ) )
    {
        char buff[256];
		snprintf(buff, 255, "Connection error: %s", connectionError->Message);
        // Abort if the connect did not succeed
        ASSERT_FAIL(buff);
    }
    char* params[1];
    params[0] = "//depot/MyCode/ReadMe.txt";

    ASSERT_INT_TRUE(ps->run_command("print", 7, 1, params, 1))

    const char* out = ps->get_ui(7)->GetTextResults();

    ASSERT_NOT_NULL(out);

    ASSERT_STRING_EQUAL(out, "Don't Read This!\n\nIt's Secret!")

    return true;
}

bool TestP4BridgeServer::TestBinaryOutCommand()
{
    P4ClientError* connectionError = NULL;
    // create a new server
    ps = new P4BridgeServer("localhost:6666", "admin", "", "admin_space");
    ASSERT_NOT_NULL(ps);

    // connect and see if the api returned an error. 
    if( !ps->connected( &connectionError ) )
    {
        char buff[256];
		snprintf(buff, 255, "Connection error: %s", connectionError->Message);
        // Abort if the connect did not succeed
        ASSERT_FAIL(buff);
    }
    char* params[1];
    params[0] = "//depot/MyCode/Silly.bmp";

    ASSERT_INT_TRUE(ps->run_command("print", 7, 1, params, 1))

    size_t cnt = ps->get_ui(7)->GetBinaryResultsCount();

    ASSERT_EQUAL(cnt, 3126)

    const unsigned char * out = ps->get_ui(7)->GetBinaryResults();

    ASSERT_NOT_NULL(out);
    ASSERT_EQUAL((*(((unsigned char*)out) + 1)), 0x4d)

    return true;
}

bool TestP4BridgeServer::TestErrorOutCommand()
{
    P4ClientError* connectionError = NULL;
    // create a new server
    ps = new P4BridgeServer("localhost:6666", "admin", "", "admin_space");
    ASSERT_NOT_NULL(ps);

    // connect and see if the api returned an error. 
    if( !ps->connected( &connectionError ) )
    {
        char buff[256];
		snprintf(buff, 255,"Connection error: %s", connectionError->Message);
        // Abort if the connect did not succeed
        ASSERT_FAIL(buff);
    }
    char* params[1];
    params[0] = "//depot/MyCode/Billy.bmp";

    // run a command against a nonexistent file
    // Should fail
    ASSERT_FALSE(ps->run_command("rent", 7, 1, params, 1))

    P4ClientError * out = ps->get_ui(7)->GetErrorResults();

    ASSERT_NOT_NULL(out);

    //ASSERT_STRING_STARTS_WITH(out->Message, "Unknown command.  Try 'p4 help' for info")
	ASSERT_EQUAL(out->ErrorCode, 805379098);
    ASSERT_NULL(out->Next)

    return true;
}

bool TestP4BridgeServer::TestGetSet()
{
	char *expected = "C:\\login.bat";

	char *test_charset = "utf16";

	// first thing: delete P4FOOBAR from the registry
	P4BridgeServer::Set("P4FOOBAR", NULL);
	P4BridgeServer::Reload();

	P4ClientError* connectionError = NULL;
	// create a new server
	ps = new P4BridgeServer("localhost:6666", "admin", "", "");
	ASSERT_NOT_NULL(ps);
	ps->SetLogCallFn((LogCallbackFn *) &TestP4BridgeServer::LogCallback);
	ASSERT_NOT_NULL(ps);

	// set server to use test_charset for file translation
	ps->set_charset(test_charset);

	// get charset from server
	string cset = ps->get_charset();

	ASSERT_STRING_EQUAL(test_charset, cset.c_str());

	// Test the P4BridgeEnviro Update() function
	// Any values set with Update should be the first returned.
	P4BridgeServer::Update("P4FOOBAR", expected);

	const char* result = P4BridgeServer::Get("P4FOOBAR");

	ASSERT_STRING_EQUAL(expected, result);

	P4BridgeServer::Reload();
	result = P4BridgeServer::Get("P4FOOBAR");
	ASSERT_NULL(result);

	wchar_t *w_expected = L"C:\\login<АБВГ>.bat";
	expected = TextEncoder::Utf16ToUtf8(w_expected);

	P4BridgeServer::Set("P4FOOBAR", expected);

	result = P4BridgeServer::Get("P4FOOBAR");

	ASSERT_STRING_EQUAL(expected, result);

	wchar_t *w_result = TextEncoder::Utf8ToUtf16(result);

	ASSERT_W_STRING_EQUAL(w_expected, w_result);

	P4BridgeServer::Set("P4FOOBAR", NULL);
	result = P4BridgeServer::Get("P4FOOBAR");
	// P4-16150, Set + Get on Windows returns cached value
	ASSERT_NULL(result);


	// calling ::Set below will change the allocation of the
	// strptr that ::Get is pointing at.  you have to cache
	// the value (string object) if you need it later
	const char* _orig = P4BridgeServer::Get("P4CONFIG");
	string orig = (_orig == NULL) ? "" : _orig;
	string actual = "not_a_real_config_setting";
	P4BridgeServer::Set("P4CONFIG", actual.c_str());
	string getResult = P4BridgeServer::Get("P4CONFIG");
	ASSERT_STRING_EQUAL(actual.c_str(), getResult.c_str());

	P4BridgeServer::Set("P4CONFIG", orig.c_str());
	const char* _getResult = P4BridgeServer::Get("P4CONFIG");
	if (!_getResult) 
	{
		ASSERT_NULL(_orig);
	}
	else 
	{
		ASSERT_STRING_EQUAL(orig.c_str(), _getResult);
	}

   return true;
}

bool TestP4BridgeServer::TestEnviro()
{
	// Save the existing value
	const char* _existing = P4BridgeServer::Get("P4CONFIG");
	string existing = (_existing) ? _existing : "";

	// Override the value with Update()
	P4BridgeServer::Update("P4CONFIG", "myP4Config.txt");
	string result1 = P4BridgeServer::Get("P4CONFIG");
	printf("result1: %s\n",result1);

	// Check with a value not in the environment
	P4BridgeServer::Update("P4NOENV", "myP4NOENV.txt");
	string result2 = P4BridgeServer::Get("P4NOENV");
	printf("result2: %s\n",result2);

	ASSERT_STRING_EQUAL(result2.c_str(), "myP4NOENV.txt");

	// Now try to clear the value locally
	P4BridgeServer::Update("P4CONFIG","");
	const char* pResult = P4BridgeServer::Get("P4CONFIG");
	ASSERT_NULL(pResult);

	// Now remove the Local override
	P4BridgeServer::Reload();
	const char* result4 = P4BridgeServer::Get("P4CONFIG");

	if (!result4) 
	{
		ASSERT_NULL(_existing);
	}
	else {
		ASSERT_STRING_EQUAL(result4, existing.c_str());
	}
	
    return true;
}

bool TestP4BridgeServer::TestConnectSetClient()
{
	ps = new P4BridgeServer("localhost:6666", "admin", NULL, NULL);
	ASSERT_NOT_NULL(ps);
	ps->set_client("admin_space");

	// check to see that the client is set correctly
	string client = ps->get_client();
	ASSERT_STRING_EQUAL(client.c_str(), "admin_space");

	return true;
}

void WriteConfigFile(const char* file)
{
	std::filebuf fb;
	fb.open(file, std::ios::out);
	std::ostream os(&fb);
	os << "P4CLIENT testClient\r\n";
	fb.close();
}

bool TestP4BridgeServer::TestGetConfig()
{
	// grab the global enviro's P4CONFIG
	const char*_oldConfig = P4BridgeServer::Get("P4CONFIG");
	if (_oldConfig == NULL || *_oldConfig == 0)
		_oldConfig = "noconfig";
	string oldConfigPath = (string(testCfgDir) + "\\" + _oldConfig);

	// write 2 config files for the enviro to pick up
	WriteConfigFile(testCfgFile);
	WriteConfigFile(oldConfigPath.c_str());

	// NOTE: Update() merely caches a value local to the underlying enviro object
	//       If you call Get() it will return that value
	//		 If you cause the enviro to reload, the cached value will be lost
	const char *oldConfig = P4BridgeServer::Get("P4CONFIG");

	P4BridgeServer::Update("P4CONFIG", "myP4Config.txt");	
	const char* cfg = P4BridgeServer::Get("P4CONFIG"); 
	ASSERT_STRING_EQUAL(cfg, "myP4Config.txt");

	// get_config should not use the "default" enviro object
	// other than to get the relevant P4CONFIG value
	// TODO: test that the P4CONFIG setting is unchanged
	string result = P4BridgeServer::get_config(testCfgDir);
	ASSERT_STRING_EQUAL(testCfgFile, result.c_str());

    P4ClientError* connectionError = NULL;
    // create a new server
    ps = new P4BridgeServer("localhost:6666", "admin", "", "admin_space");
    ASSERT_NOT_NULL(ps);

	// set the cwd to null (should not fail)
	ps->set_cwd(NULL);

	// this will reset ps's enviro, which means P4CONFIG will be the default again
	// but the "global" one should remain the same
	ps->set_cwd(testCfgDir);
	cfg = P4BridgeServer::Get("P4CONFIG");
	ASSERT_STRING_EQUAL("myP4Config.txt", cfg);

	result = ps->get_config();

	if (strcmp(_oldConfig, "noconfig") != 0)
	{
		ASSERT_STRING_EQUAL(oldConfigPath.c_str(), result.c_str());
	}
	else
	{
		ASSERT_STRING_EQUAL(_oldConfig, result.c_str());
	}

    return true;
}

bool TestP4BridgeServer::TestIsIgnored()
{
	OFSTRUCT ReOpenBuff;

	const char* pIgnore = P4BridgeServer::Get("P4IGNORE");
	oldIgnore = (pIgnore ? pIgnore : "");

	HFILE hf = OpenFile(testIgnoreFile, &ReOpenBuff, OF_CREATE);
	DWORD written = 0;
	WriteFile((HANDLE) hf, (LPVOID) "foofoofoo.foo\r\n", 15, &written, NULL);
	CloseHandle((HANDLE) hf);

	P4BridgeServer::Set("P4IGNORE", "myP4Ignore.txt");	

	ASSERT_INT_TRUE(P4BridgeServer::IsIgnored(StrRef(testIgnoredFile1)));
	ASSERT_FALSE(P4BridgeServer::IsIgnored(StrRef(testIgnoredFile2)));

	return true;
}

//    Some experimentation with command options happened in this test.
//     There is a lot of supporting code which is still there, but unneeded for just the test
bool TestP4BridgeServer::TestSetVars()
{
	Error e;

	// test data for reading Protocols (these are read after client::Init())
	char *name = "apilevel";
	int value = 38;

	char *name1 = "server";
	char *value1 = "40";

	char *name2 = "nocase";
	char *value2 = "1";

	char *name3 = "security";
	char *value3 = "0";

	char *name4 = "unicode";
	char *value4 = "0";
 
	P4ClientError* connectionError = NULL;
	StrPtr *pptr = NULL;

	// create a new server
	ps = new P4BridgeServer("localhost:6666", "admin", "", "admin_space");
	ASSERT_NOT_NULL(ps);

	// enable logging of C++ events
	//ps->SetLogCallFn((LogCallbackFn *)&TestP4BridgeServer::LogCallback);  // Works
		
	// Protocol options must be set before client::Init()
	// -Z options are set by Protocol
	//ps->SetProtocol("dbstat","");    // "track", "dbstat", "app=name", "proxyverbose"
	//ps->SetProtocol(P4Tag::v_tag, "yes");  // enable tagged output  WORKS
	// ps->SetProtocol("track", ""); // enable tracking WORKS

	// connect and see if the api returned an error. 
	if (!ps->connected(&connectionError))
	{
		char buff[256];
		snprintf(buff, 255, "Connection error: %s", connectionError->Message);
		// Abort if the connect did not succeed
		ASSERT_FAIL(buff);
	}

	P4Connection* client = ps->getConnection(0);
	P4BridgeClient *bridge_ui = ps->get_ui(0);

	// What command to send?
	char * cmdname = "users";
	char * const v[2] = {"-m", "10"};
	client->SetArgv(2, v);

	// Interestingly,  the "tag" option works as both a Var and a Protocol
	//client->SetVar(P4Tag::v_tag, "yes");  // enable tagged output  WORKS

	// -z options maxLockTime maxOpenFiles maxResults maxScanRows
	//client->SetVarV("maxOpenFiles=1");  

	// -v options are passed by p4debug.SetLevel() calls.
	// p4debug.SetLevel("rpc=3");   // This will spew debug output to stdout WORKS
	// p4debug.SetLevel(5);		// This will spew ALL debug output to stdout WORKS

	//p4debug.SetLevel("track=1");

	ps->Run_int(client, cmdname, bridge_ui);

	//p4debug.SetLevel(0);   // Disable Debug output WORKS

	StrDictListIterator * tagout = bridge_ui->GetTaggedOutput();	
	const char* tout = bridge_ui->GetTextResults();	
	StrPtr * tcount = bridge_ui->GetDataSet();

	const unsigned char * bout = bridge_ui->GetBinaryResults();
	size_t bcount = bridge_ui->GetBinaryResultsCount();

	P4ClientInfoMsg *iout = bridge_ui->GetInfoResults();
	int icount = bridge_ui->GetInfoResultsCount();
	P4ClientError *eout = bridge_ui->GetErrorResults();

	if (icount > 0 && iout)
	{
		P4ClientInfoMsg *imsg = iout;
		while (imsg)
		{
			printf("info: %d %s\n", imsg->Level, imsg->Message);
			imsg = imsg->Next;
		}
			
	}
	//ASSERT_NOT_NULL(tagout);

	if (tagout)  // Tagged output
	{
		int itemCnt = 0;
		while (StrDictList * pItem = tagout->GetNextItem())
		{
			int entryCnt = 0;

			while (KeyValuePair * pEntry = tagout->GetNextEntry())
			{
				printf("tag: %d %s => %s\n", itemCnt, pEntry->key, pEntry->value);
				entryCnt++;
			}
			itemCnt++;
		}
	}

	if (tout) // Text output
	{
		printf("text: %s", tout);
	}


	int level = ps->APILevel();

	if (level < value)   // Server2 38 or larger is ok
	{
		char buff[256];
		snprintf(buff, 255, "API mismatch %d < %d", level, value);
		ASSERT_FAIL(buff);
	}

	pptr = client->GetProtocol(name1);
	ASSERT_NOT_NULL(pptr);

	if (strcmp(value1, pptr->Value()))
	{
		char buff[256];
		snprintf(buff, 255, "Value1 mismatch %s != %s", value1, pptr->Value());
		ASSERT_FAIL(buff);
	}

	pptr = client->GetProtocol(name2);
	ASSERT_NOT_NULL(pptr);

	if (strcmp(value2, pptr->Value()))
	{
		char buff[256];
		snprintf(buff, 255, "Value2 mismatch %s != %s", value2, pptr->Value());
		ASSERT_FAIL(buff);
	}

	pptr = client->GetProtocol(name3);
	ASSERT_NOT_NULL(pptr);

	if (strcmp(value3, pptr->Value()))
	{
		char buff[256];
		snprintf(buff, 255, "Value3 mismatch %s != %s", value3, pptr->Value());
		ASSERT_FAIL(buff);
	}

	pptr = client->GetProtocol(name4);
	ASSERT_NOT_NULL(pptr);

	if (strcmp(value4, pptr->Value()))
	{
		char buff[256];
		snprintf(buff, 255, "Value4 mismatch %s != %s", value4, pptr->Value());
		ASSERT_FAIL(buff);
	}
	return true;
}

// A test for Parallel Sync error handling - Job085941

bool TestP4BridgeServer::TestParallelSync()
{
    P4ClientError* connectionError = NULL;
    // create a new server
    ps = new P4BridgeServer("localhost:6666", "admin", "", "admin_space");
    ASSERT_NOT_NULL(ps);

    // connect and see if the api returned an error. 
    if( !ps->connected( &connectionError ) )
    {
        char buff[256];
		snprintf(buff, 255, "Connection error: %s", connectionError->Message);
        // Abort if the connect did not succeed
        ASSERT_FAIL(buff);
    }

    ASSERT_NOT_EQUAL(ps->unicodeServer(), 1);
   // ps->set_charset("utf8", "utf16le");

	char *args[4];

	// run p4 configure set net.parallel.max=4	
	args[0] = "set";
	args[1] = "net.parallel.max=4";
	ASSERT_INT_TRUE(ps->run_command("configure", 0, 1, args, 2));

	// run p4 configure set dmc=3
	args[0] = "set";
	args[1] = "dmc=3";
	ASSERT_INT_TRUE(ps->run_command("configure", 0, 1, args, 2));

	// Paths of interest for the parallel test.
	std::string ppath = "c:\\MyTestDir\\admin_space\\TestData\\parallel\\";
	std::string ppathall = ppath + "...";
	std::string ppathnone = ppathall + "#none";
	std::string targetpath = ppath + "testFile99.txt";

	std::string ppath1 = "c:\\MyTestDir\\admin_space2\\TestData\\parallel\\";
	std::string targetpath1 = ppath1 + "testFile99.txt";

	// Create the workspace directory for the parallel test
	CreateDirectory(ppath.c_str(), NULL);
	
	// write 500 files of 1K size to the workspace directory
	int count = 500;
	for (int i = 0; i < count; i++)
	{
		std::ostringstream oss;
		oss << ppath << "testFile" << i << ".txt";

		std::ofstream ofs(oss.str(),std::ios::binary | std::ios::out);
		ofs.seekp(1020);
		ofs.write("test", 4);
		ofs.close();
	}

	// add them to the server
	args[0] = const_cast<char *>(ppathall.c_str());
	ASSERT_INT_TRUE(ps->run_command("add",0,1,args,1));

	// submit them
	args[0] = "-d";
	args[1] = "\"initial submit of test files\"";
	args[2] = const_cast<char *>(ppathall.c_str());
	ASSERT_INT_TRUE(ps->run_command("submit",0,1,args,3));

	// Remove all Workspace Files (Sync to NONE)
	args[0] = "-f";
	args[1] = const_cast<char *>(ppathnone.c_str());
	ASSERT_INT_TRUE(ps->run_command("sync",0,1,args,2));

	// Sync just the target file
	args[0] = "-f";
	args[1] = const_cast<char *>(targetpath.c_str());
	ASSERT_INT_TRUE(ps->run_command("sync",0,1,args,2));

	// Change target file to Writable
	_chmod(targetpath.c_str(), _S_IWRITE | _S_IREAD);

	// Save current user and client
	//const StrPtr *user1 = ps->get_user();
	//const StrPtr *client1 = ps->get_client();

	// Change to a different client
	ps->set_client("admin_space2");

	// Sync target file
	args[0] = "-f";
	args[1] = const_cast<char *>(targetpath1.c_str());
	ASSERT_INT_TRUE(ps->run_command("sync",0,1,args,2));

	// Edit the target file
	args[0] = const_cast<char *>(targetpath1.c_str());
	ASSERT_INT_TRUE(ps->run_command("edit",0,1,args,1));

	// Submit the target file
	args[0] = "-d";
	args[1] = "\"submit of test file\"";
	args[2] = const_cast<char *>(targetpath1.c_str());
	ASSERT_INT_TRUE(ps->run_command("submit",0,1,args,3));

	// Back to the original workspace
	ps->set_client("admin_space");

	// Do a parallel sync of the original workspace, causes overwrite error, should fail
	args[0] = "--parallel";
	args[1] = "threads=4,batch=8,batchsize=2000,min=9,minsize=2000";
	args[2] = const_cast<char *>(ppathall.c_str());
	int rv = ps->run_command("sync",0,1,args,3);
	ASSERT_INT_FALSE(rv);  // Expect failure

    return true;
}

int __stdcall MyParallelSyncCallbackFn(int* pServer, const char *cmd, const char** pArgs, int argCount, int* dictListIter, int threads)
{
	// stub: print the args out
	printf("P4BridgeServer: 0x[0x%llu]\n", pServer);
	printf("Command: %s\n", cmd);
	printf("Args:\n");
	for (int i = 0; i < argCount; i++) 
	{
		printf("\t%d: %s\n", i, pArgs[i]);
	}
	printf("Dictionary:\n");
	StrDictListIterator* pDli = (StrDictListIterator*)dictListIter;
	KeyValuePair* pCur = pDli->GetNextEntry();
	while (pCur) 
	{
		printf("\t%s: %s\n", pCur->key.c_str(), pCur->value.c_str());
		pCur = pDli->GetNextEntry();
	}
	printf("Threads: %d\n", threads);

	// TODO: fill in the param checking from the real callback
	//		 we could manually launch p4.exe to run the requested
	//		 operations and verify that it worked?

	// for now just return OK
	return 0;
}

// similar to above but with light callback testing
bool TestP4BridgeServer::TestParallelSyncCallback()
{
	{
		// run p4 configure set net.parallel.max=4	
		P4BridgeServer* pServer = new P4BridgeServer("localhost:6666", "admin", "", "admin_space");
		char *args[4];
		args[0] = "set";
		args[1] = "net.parallel.max=4";
		ASSERT_INT_TRUE(pServer->run_command("configure", 0, 1, args, 2));

		// run p4 configure set dmc=3 (?)
		args[0] = "set";
		args[1] = "dmc=3";
		ASSERT_INT_TRUE(pServer->run_command("configure", 0, 1, args, 2));

		delete pServer;
	}
	
	// make a new connection to pick up the net.parallel.max value
	P4BridgeServer* pServer = new P4BridgeServer("localhost:6666", "admin", "", "admin_space");
	P4Connection* pCon = pServer->getConnection(7);
	P4BridgeClient * ui = pCon->getUi();

	pServer->SetParallelTransferCallbackFn(MyParallelSyncCallbackFn);

	// positive test here; in this phase it should
	//   - call p4 sync with --parallel set to something useful
	//   - receive a callback with some text message
	//   - the sync will actually fail (because we did not sync anything)
	const char* args[] = {
		"--parallel",
		"threads=4,batch=8,batchsize=1,min=1,minsize=1",
		"-f",
		"//..."
	};

	ASSERT_INT_TRUE(pServer->run_command("sync", 7, true, (char**)args, sizeof(args)/sizeof(args[0])));
	// verify some of the files are here
	
	StrDictListIterator * tagged = pServer->get_ui(7)->GetTaggedOutput();
	int itemCnt = 0;
	while (StrDictList * pItem = tagged->GetNextItem())
	{
		int entryCnt = 0;

		// Find each local filename from tagged output
		// and confirm that it exists
		while (KeyValuePair * pEntry = tagged->GetNextEntry())
		{
			struct stat buffer;
			if (strcmp(pEntry->key.c_str(), "clientFile") == 0)
				ASSERT_INT_TRUE((stat(pEntry->value.c_str(), &buffer) == 0));
		}
		itemCnt++;
	}

	delete tagged;

	// negative tests
	// set a null pointer, which means...use p4.exe instead?
	pServer->SetParallelTransferCallbackFn((ParallelTransferCallbackFn *)0x00);
	ASSERT_INT_TRUE(pServer->run_command("sync", 7, true, (char**)args, sizeof(args) / sizeof(args[0])));

	// set an invalid pointer, which will fail but gracefully
	pServer->SetParallelTransferCallbackFn((ParallelTransferCallbackFn *)0xFFFFFFFF);
	ASSERT_INT_FALSE(pServer->run_command("sync", 7, true, (char**)args, sizeof(args) / sizeof(args[0])));

	// now that Client's "errors" member has incremented, run sync again without parallel to make sure that 
	// we don't get an error for a successful run without disconnect
	const char* args2[] = { "-f", "//..." };

	ASSERT_INT_TRUE(pServer->run_command("sync", 7, true, (char**)args2, sizeof(args2) / sizeof(args2[0])));

	delete pServer;

	return true;
}

bool TestP4BridgeServer::TestDefaultProgramNameAndVersion()
{
    P4ClientError* connectionError = NULL;
    // create a new server
    ps = new P4BridgeServer("localhost:6666", "admin", "", "admin_space");
    ASSERT_NOT_NULL(ps);

    // connect and see if the api returned an error. 
    if( !ps->connected( &connectionError ) )
    {
        char buff[256];
		snprintf(buff, 255, "Connection error: %s", connectionError->Message);
        // Abort if the connect did not succeed
        ASSERT_FAIL(buff);
    }
    char* params[1];
    params[0] = "//depot/mycode/*";

	//p4base::DumpMemoryState("Before first command run");

	ASSERT_INT_TRUE(ps->run_command("files", 6, 1, params, 1))

	// should both be set to default
	ASSERT_STRING_EQUAL(ps->pProgramName.c_str(), "something Application");
	ASSERT_STRING_EQUAL(ps->pProgramVer.c_str(), "1, 0, 0, 1");

	//p4base::DumpMemoryState("After first command run");

	// set both program name and version
	ps->pProgramName = testProgramName;
	ps->pProgramVer = testProgramVer;

	ASSERT_INT_TRUE(ps->run_command("files", 7, 1, params, 1))

	// should both be set to supplied strings
	ASSERT_STRING_EQUAL(ps->pProgramName.c_str(), testProgramName);
	ASSERT_STRING_EQUAL(ps->pProgramVer.c_str(), testProgramVer);
	//p4base::DumpMemoryState("After first command run");

	// set program name but not version
	ps->pProgramName = testProgramName;
	ps->pProgramVer.clear();

	ASSERT_INT_TRUE(ps->run_command("files", 8, 1, params, 1))

	// only name should  be set to supplied strings
	ASSERT_STRING_EQUAL(ps->pProgramName.c_str(), testProgramName);
	ASSERT_STRING_NOT_EQUAL(ps->pProgramVer.c_str(), testProgramVer);
	ASSERT_STRING_EQUAL(ps->pProgramVer.c_str(), "1, 0, 0, 1");

	// set program version but not name
	ps->pProgramName.clear();
	ps->pProgramVer = testProgramVer;

	ASSERT_INT_TRUE(ps->run_command("files", 7, 1, params, 1))

	// only version should  be set to supplied strings
	ASSERT_STRING_NOT_EQUAL(ps->pProgramName.c_str(), testProgramName);
	ASSERT_STRING_EQUAL(ps->pProgramVer.c_str(), testProgramVer);
	ASSERT_STRING_EQUAL(ps->pProgramName.c_str(), "p4bridge-unit-test.exe");

    return true;
}

bool TestP4BridgeServer::TestGetTicketFile()
{
	string tktfile = P4BridgeServer::GetTicketFile();

	ASSERT_TRUE(tktfile.length() != 0);

	return true;
}

// Callback to provide password for "login" in TestUnicodeUserName()
void _stdcall ProvidePassword2(int cmdId, const char *message, char *response, int responseSize, int noEcho)
{
	strncpy(response, "pass1234", responseSize);
}

bool TestP4BridgeServer::TestSetTicketFile()
{
	ps = new P4BridgeServer("localhost:6666", "admin", "", "admin_space");
	ASSERT_NOT_NULL(ps);

	P4ClientError* connectionError = NULL;

	if (!ps->connected(&connectionError))
	{
		char buff[256];
		snprintf(buff, 255, "Connection error: %s", connectionError->Message);
		// Abort if the connect did not succeed
		ASSERT_FAIL(buff);
	}
	char* params[] = { "set","security=2" };
	ASSERT_INT_TRUE(ps->run_command("configure", 7, 0, params, 2));
	//string tktfile = ps->ticketFile;
	//ASSERT_TRUE(tktfile.length() != 0);
	//ASSERT_EQUAL(tktfile, testTicketFile);

	ps->SetPromptCallbackFn(ProvidePassword2);
	//char* params2[] = { "-P","pass1234" };
	ASSERT_INT_TRUE(ps->run_command("passwd", 7, 0, NULL, 0));

    delete ps;

	remove(testTicketFile);


	ps = new P4BridgeServer("localhost:6666", "admin", "pass1234", "admin_space");
	ps->set_ticketFile(testTicketFile);

	if (!ps->connected(&connectionError))
	{
		char buff[256];
		snprintf(buff, 255, "Connection error: %s", connectionError->Message);
		// Abort if the connect did not succeed
		ASSERT_FAIL(buff);
	}
	ps->SetPromptCallbackFn(ProvidePassword2);
	ASSERT_INT_TRUE(ps->run_command("login", 7, 0, NULL, 0));

	ASSERT_INT_TRUE(ps->run_command("depots", 7, 0, NULL, 0));

	std::filebuf fb;
	ASSERT_NOT_NULL(fb.open(testTicketFile, std::ios::in));


		
	return true;
}


bool FindInLog(std::string val)
{
	// scan through the server log looking for 'RpcRecvBuffer pizza = 333333'
	std::filebuf fb;
	fb.open(TestLog, std::ios::in);
	std::istream is(&fb);
	bool foundIt = false;
	char buff[4096];
	while (!is.eof())
	{
		is.getline(buff, sizeof(buff));
		if (val == buff)
		{
			return true;
		}
	}
	return false;
}

bool TestP4BridgeServer::TestSetProtocol()
{
	P4ClientError* connectionError = NULL;

	// turn on rpc=3 on the server and reconnect
	{
		char* args[] = { "set", "rpc=3" };
		P4BridgeServer* pServer = new P4BridgeServer("localhost:6666", "admin", "", "admin_space");
		ASSERT_TRUE(pServer->connected(&connectionError) == 1);
		ASSERT_INT_TRUE(pServer->run_command("configure", 0, true, args, 2));
		delete pServer;
	}
	
	// create a another connection
	ps = new P4BridgeServer("localhost:6666", "admin", "", "admin_space");
	ASSERT_NOT_NULL(ps);

	// must run SetProtocol before the connection is established
	// note that for the most part SetProtocol is already handled 
	// in our connection code, but this is for the case when
	// a parallel sync/transmit response has protocol data for
	// the new connections

	// note that the server needs to be started with rpc=3
	// set a bogus protocol
	ps->SetProtocol("pizza", "333333");
	// set an api number too
	ps->SetProtocol("api", "9090909");
	
	// connect and see if the api returned an error. 
	if (!ps->connected(&connectionError))
	{
		char buff[256];
		snprintf(buff, 255, "Connection error: %s", connectionError->Message);
		// Abort if the connect did not succeed
		ASSERT_FAIL(buff);
	}

	ASSERT_INT_TRUE(ps->run_command("info", 0, true, NULL, 0));
	// pizza is only in the RpcRecvBuffer, the server will ignore it
	ASSERT_TRUE(FindInLog("RpcRecvBuffer pizza = 333333"));
	ASSERT_TRUE(FindInLog("RpcRecvBuffer api = 9090909"));

	// negative test: call SetProtocol too late (we're connected)
	ps->SetProtocol("calzone", "444444");
	ASSERT_INT_TRUE(ps->run_command("info", 0, true, NULL, 0));
	ASSERT_FALSE(FindInLog("RpcRecvBuffer calzone = 444444"));

	// positive test: reconnnect and check again
	ASSERT_INT_TRUE(ps->disconnect());
	ASSERT_TRUE(ps->connected(&connectionError) == 1);
	ASSERT_INT_TRUE(ps->run_command("info", 0, true, NULL, 0));
	ASSERT_TRUE(FindInLog("RpcRecvBuffer calzone = 444444"));

	return true;
}
