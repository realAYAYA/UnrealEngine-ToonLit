WebTests
--------------------

This is a collection of tests for web features like http, websocket, ssl etc.

# Running the Tests

## Run the WebTestsServer(NOTE: This server is created for TEST PURPOSE ONLY, it's not production ready!!!):
	In ../WebTestsServer folder:
		> runserver.bat
	Or if you use docker:
		> dockerbuildandrun.bat

## Run the tests from VS:
	Set `WebTests` as the startup project and set Solution Configuration to a `Development`.
	If running tests on other devices, pass in the ip as command line args, after extra args AT THE END, like: "--extra-args --web_server_ip=your.pc.ip.address"
	Compile and debug

# Adding new test case in WebTestsServer:
	Add/change the code in ./WebTestsServer code, most likely in httptests/urls.py and httptests/views.py, and save. Code will be reloaded if the web server is running
	When using docker, also need to run dockerbuildandrun.bat
	For more info about how to code in django, check https://docs.djangoproject.com/en/
