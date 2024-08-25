// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Math/NumericLimits.h"
#include "HttpServerModule.h"
#include "IHttpRouter.h"
#include "HttpRouteHandle.h"
#include "HttpRequestHandler.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "HttpPath.h"
#include "HttpRequestHandlerRegistrar.h"
#include "HttpRequestHandlerIterator.h"


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHttpServerIntegrationTest, "System.Online.HttpServer.Integration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHttpServerIntegrationTest::RunTest(const FString& Parameters)
{
	const uint32 HttpRouterPort = 8888;
	const uint32 InvalidHttpRouterPort = TNumericLimits<uint16>::Max() + 1; // 65536
	const FHttpPath HttpPath(TEXT("/TestHttpServer"));

	// Ensure router creation
	TSharedPtr<IHttpRouter> HttpRouter = FHttpServerModule::Get().GetHttpRouter(HttpRouterPort);
	TestTrue(TEXT("HttpRouter.IsValid()"), HttpRouter.IsValid());

	// Ensure unique routers per-port
	TSharedPtr<IHttpRouter> DuplicateHttpRouter = FHttpServerModule::Get().GetHttpRouter(HttpRouterPort);
	TestEqual(TEXT("HttpRouter Duplicates"), HttpRouter, DuplicateHttpRouter);

	// Ensure failed port binds still return a valid router if not explicitly requested to fail (and by default)
	TSharedPtr<IHttpRouter> ValidHttpRouterOnFail = FHttpServerModule::Get().GetHttpRouter(InvalidHttpRouterPort /*, bFailOnBindFailure = false */);
	TestTrue(TEXT("HttpRouter is NOT null on bind failure by default"), ValidHttpRouterOnFail.IsValid());

	// Ensure we can create route bindings
	const FHttpRequestHandler RequestHandler = FHttpRequestHandler::CreateLambda([](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
	{
		return true;
	});
	FHttpRouteHandle HttpRouteHandle = HttpRouter->BindRoute(HttpPath, EHttpServerRequestVerbs::VERB_GET, RequestHandler);
	TestTrue(TEXT("HttpRouteHandle.IsValid()"), HttpRouteHandle.IsValid());

	// Disallow duplicate route bindings
	FHttpRouteHandle DuplicateHandle = HttpRouter->BindRoute(HttpPath, EHttpServerRequestVerbs::VERB_GET, RequestHandler);
	TestFalse(TEXT("HttpRouteHandle Duplicated"), DuplicateHandle.IsValid());

	// Because of the ValidHttpRouterOnFail was created by FHttpServerModule::Get().GetHttpRouter(InvalidHttpRouterPort...), it will fail to listen in StartAllListeners
	// Also after bHttpListenersEnabled got set to true by StartAllListeners, when call GetHttpRouter(InvalidHttpRouterPort...) again, it will call StartListening again in there and fail
	AddExpectedError(TEXT("HttpListener detected invalid port"), EAutomationExpectedErrorFlags::Contains, 2);
	FHttpServerModule::Get().StartAllListeners();

	// Because of the ValidHttpRouterOnFail was created by FHttpServerModule::Get().GetHttpRouter(InvalidHttpRouterPort...)
	AddExpectedError(TEXT("is not listening/bound and listeners are still enabled"), EAutomationExpectedErrorFlags::Contains, 1);
	// Ensure failed port binds result in a null router instance if requested (and listeners are enabled)
	TSharedPtr<IHttpRouter> InvalidHttpRouterOnFail = FHttpServerModule::Get().GetHttpRouter(InvalidHttpRouterPort, /* bFailOnBindFailure = */ true);
	TestFalse(TEXT("HttpRouter is null on bind failure if requested"), InvalidHttpRouterOnFail.IsValid());

	// Make a request
	/*
	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("HTTP TEST 1 http://localhost:8888/TestHttpServer")));
	*/

	FHttpServerModule::Get().StopAllListeners();

	HttpRouter->UnbindRoute(HttpRouteHandle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHttpServerPathParametersTest, "System.Online.HttpServer.PathParameters", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHttpServerPathParametersTest::RunTest(const FString& Parameters)
{
	enum EExpectedQueryResult
	{
		ShouldMatch,
		ShouldNotMatch
	};

	using EVerb = EHttpServerRequestVerbs;

	struct FHttpPathTest
	{
		FHttpPathTest(EVerb InQueryVerb, FString InTestQuery, EVerb InTargetRouteVerb, FString InTargetRoute, EExpectedQueryResult InExpectedResult)
			: QueryVerb(InQueryVerb)
			, TestQuery(MoveTemp(InTestQuery))
			, TargetRouteVerb(InTargetRouteVerb)
			, TargetRoute(MoveTemp(InTargetRoute))
			, bRouteQueried(false)
			, bExpectedResult(InExpectedResult)
		{
		}

		void SetHandler()
		{
			Handler = FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest&, const FHttpResultCallback&)
			{
				bRouteQueried = true;
				return true;
			});
		}

		EVerb QueryVerb;
		FString TestQuery;
		EVerb TargetRouteVerb;
		FHttpPath TargetRoute;
		FHttpRequestHandler Handler;

		bool bRouteQueried;
		EExpectedQueryResult bExpectedResult;
	};

	auto Callback = [](TUniquePtr<FHttpServerResponse>) {};

	TArray<FHttpPathTest> Tests;

	// Test simple static route.
	Tests.Emplace(EVerb::VERB_GET,     TEXT("/staticroute"),				    EVerb::VERB_GET, TEXT("/staticroute"),				       ShouldMatch);
	Tests.Emplace(EVerb::VERB_GET,     TEXT("/"),							    EVerb::VERB_GET, TEXT("/"),						      	   ShouldMatch);

	// Test verbs.
	Tests.Emplace(EVerb::VERB_PUT,     TEXT("/putroute"),				        EVerb::VERB_PUT, TEXT("/putroute"),					       ShouldMatch);
	Tests.Emplace(EVerb::VERB_GET,     TEXT("/otherputroute"),					EVerb::VERB_PUT, TEXT("/otherputroute"),		       	   ShouldNotMatch);
	Tests.Emplace(EVerb::VERB_PUT,     TEXT("/putdelete"),						EVerb::VERB_PUT | EVerb::VERB_DELETE, TEXT("/putdelete"),  ShouldMatch);
	Tests.Emplace(EVerb::VERB_DELETE,  TEXT("/putdelete2"),						EVerb::VERB_PUT | EVerb::VERB_DELETE, TEXT("/putdelete2"), ShouldMatch);

	// Test path parameters.
	Tests.Emplace(EVerb::VERB_GET,     TEXT("/test/parm1value"),				EVerb::VERB_GET, TEXT("/test/:parm1"),					   ShouldMatch);
	Tests.Emplace(EVerb::VERB_GET,     TEXT("/test/p1v/test2/p2v"),				EVerb::VERB_GET, TEXT("/test/:parm1/test2/:parm2"),		   ShouldMatch);
	Tests.Emplace(EVerb::VERB_GET,     TEXT("/test/p1v/test2/p2v"),				EVerb::VERB_GET, TEXT("/test/:parm1/test2/other"),         ShouldNotMatch);
	Tests.Emplace(EVerb::VERB_GET,     TEXT("/onevalue/twovalue/threevalue"),	EVerb::VERB_GET, TEXT("/:a/:b/:c"),					       ShouldMatch);
	
	// Special case: static routes should match before dynamic routes.
	Tests.Emplace(EVerb::VERB_GET,     TEXT("/route/static"),					EVerb::VERB_GET, TEXT("/route/static"),				       ShouldMatch);
	Tests.Emplace(EVerb::VERB_GET,     TEXT("/route/static"),					EVerb::VERB_GET, TEXT("/route/:dynamic"),			 	   ShouldNotMatch);

	// Test path parameter with verbs
	Tests.Emplace(EVerb::VERB_PUT,	 TEXT("/rt/static"),						EVerb::VERB_GET | EVerb::VERB_PUT, TEXT("/rt/:dynamic"),   ShouldMatch);

	FHttpRequestHandlerRegistrar Registrar;

	for (FHttpPathTest& Test : Tests)
	{
		Test.SetHandler();
		Registrar.AddRoute(MakeShared<FHttpRouteHandleInternal>(Test.TargetRoute.GetPath(), Test.TargetRouteVerb, Test.Handler));
	}

	for (FHttpPathTest& Test : Tests)
	{
		// In case this route was queried by some other test.
		Test.bRouteQueried = false;

		TSharedPtr<FHttpServerRequest> Request = MakeShared<FHttpServerRequest>();
		Request->Verb = Test.QueryVerb;
		Request->RelativePath = Test.TestQuery;

		FHttpRequestHandlerIterator Iterator(Request, Registrar);
		if (const FHttpRequestHandler* RequestHandlerPtr = Iterator.Next())
		{
			[[maybe_unused]] bool bHandled = RequestHandlerPtr->Execute(*Request, Callback);
		}

		if ((Test.bExpectedResult == ShouldMatch && !Test.bRouteQueried) || (Test.bExpectedResult == ShouldNotMatch && Test.bRouteQueried))
		{
			AddError(FString::Printf(TEXT("Expected query %s to have result: %s when targeting route %s"), *Test.TestQuery, Test.bExpectedResult ? TEXT("true") : TEXT("false"), *Test.TargetRoute.GetPath()), 1);
		}
	}

	return true;
}