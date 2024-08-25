# Release Notes

## 2024-03-28

* Remove /userinfo call when authenticating via JWT. If an access token is passed, it's not guaranteed it has permission to access /userinfo from OIDC. ID tokens during normal web-based login does on the other hand. (32539012)
* Fix parsing of true/false literals in condition expressions. (32518640)
* Add a default pool to include interactive agents, and map it to the TestWin64 agent type. (32514202)
* Add logging to trace when blobs are scanned for imports, to help debug some logs being expired while still referenced. (32506875)
* Normalize keys for artifacts to allow case-insensitive searching. (32495123)
* Support setting arbitrary artifact metadata. This cannot be queried, but will be returned in artifact responses. (32464180)
* Run a background task to monitor for hangs when executing Perforce commands. (32461961)
* Allow configuring different telemetry stores for each project and stream. (32460547)
* Fix slow queries in storage service garbage collection. (32460451)
* Add a [TryAuthorize] attribute which attempts authorization, but which does not return a challenge or forbid result on failure. Should fix tool download requests using service account tokens. (32447487)
* Fix OIDC issues when using Horde auth with UBA. (32406994)

## 2024-03-21

* Lower log level of failed AWS instance start (32397273)
* Assume any exceptions while checking for config files being out of date as meaning they need to be read again. (32388229)
* Use the first specified credentials for server P4 commands if the serviceAccount property is not set. (32383940)
* Update docs for deploying server on Mac. (32370724)
* Log a warning whenever an obsolete endpoint is used. (32329528)
* Fix separate RAM slots being reported separately in agent properties. (32328916)
* Fix use of auth tokens from environment variables when the server URL is also derived from environment variables. (32305895)
* Fix StorageService not being registered as a hosted service, preventing GC from running. (32291615)
* Fix artifacts not being added to graphs. (32286255)
* Add output artifacts to the graph from the exported BuildGraph definition. (32281784)

## 2024-03-15

* Get the userinfo endpoint from the OIDC discovery document rather than assuming it's a fixed path. (32250458)
* Fix serialization issue with global config path on Linux (32240996)
* Improve error reporting when exceptions are thrown during leases. (32233035)
* Prevent DeadlineExceeded exceptions in log rpc task from being logged as errors. (32232976)
* Update EpicGames.Perforce.Native to use the 2023.2 Perforce libraries. (32232242)
* Enable notification service on all server types, otherwise we keep queuing up new tasks without ever removing them. (32220252)
* Invalidate the current session when deleting an agent from the farm. (32212590)
* Invalidate registrations with a server if creating a session fails with a permission denied error. (32212298)
* Fix job costs displaying for all users. (32154404)
* Expose the bundle page size as a configurable option. Still defaults to 1mb. (32153263)
* Use BitFaster.Caching for the LRU bundle cache. (32153088)
* Pool connections in ManagedWorkspace rather than creating a new instance each time. (32144653)
* Exclude appsettings.Local.json files from publish output folders. (32140083)
* Add a UGS config file to command line tool. (32120883)
* Add endpoints to check permissions for certain ACL actions in different scopes. (32118158)
* Immediately cancel running leases if agent status gets set to busy (paused) (32102044)
* Include a separate file containing default values for Horde configuration from the a file copy into C:\ProgramData. (32098391)
* Add OAuth/OIDC support for internal Horde accounts. (32097065)

## 2024-03-07

* Store the URL of the default Horde server in the registry on Windows (31994865)
* Download UGS deployment settings from Horde parameters endpoint (32001513)
* Allow using partitioned workspaces for P4 replication (32041630)
* Allow only keeping a fixed number of artifacts rather than expiring by time (32043455)
* Include dashboard web UI files in public Docker image for Horde server (32054568)
* Turn off native Perforce client on non-Windows agents (31991203)

## 2024-02-29

* Add a back-off delay when RPC client gets interrupted in JsonRpcLogSink (31881762)
* Add a manual approval process for agents joining the farm. New EnrollmentRpc endpoint allows agents to connect and wait for approval without being assigned an agent id or being able to take on work. (31869942)
* Fix bug with tray app not setting agent in paused state during idle (31837900)
* Copy the tray app to a folder under C:\Users\xxx\AppData\Local before running, so the agent can update without having to restart the tray app (which may be running under a different user account). (31815878)
* Use the agent's reported UpdateChannel property to choose which tool to use for updating the installation. ()
* Write the name of the channel to use for agent updates into the agent config file, and report it to the server through the agent properties. (31806993, 31810088)
* Separate implementations for regular user accounts from service accounts. (31802961)
* Added endpoints for querying entitlements of the current user (or any other account). /account/entitlements will return entitlements for the logged in user, and /api/v1/accounts/xxx/entitlements will return entitlements for a Horde account. (31777667)
* Accounts now use the user's full name for the standard OIDC name claim. (https://openid.net/specs/openid-connect-core-1_0.html#StandardClaims) (31773722)

## 2024-02-23

* Fix regression in UGS metadata filtering, where metadata entries with an empty project string should be returned for any project. (31741737)
* Artifact expiry times are now retroactively updated whenever the configured expiry time changes. (31736576)
* Allow specifying a description string for artifacts. (31729492)
* Disable internal Horde account login by default. (31724108)
* Fix bundled tools not being handled correctly in installed builds. (31721936)
* Read registry config first so files and env vars can override (31720705)

## 2024-02-22

* Fixed bundled tools not being handled correctly in installed builds. (31721936)
* Read registry config first so files and env vars can override (31720705)
* Fix leases not being cancelled when a batch moves into the NoLongerNeeded state. (31709570)
* Prevent Windows service startup from completing until all Horde services have started correctly. (31691578)
* Add firewall exception in installer (31678384, 31677487)
* Always return a mime type of text/plain for .log files, allowing them to be viewed in-browser (31648543)
* Configure HTTP/2 port in server installer (31647511)
* Enable new temp storage backend by default. (31637506)
* Support for defining ACL profiles which can be shared between different ACL entries. Intent is to allow configuring some standard permission sets which can be inherited and modified by users. Two default profiles exist - 'generic-read' and 'generic-run' - which match how we have roles configured internally. New profiles can be created in the same scopes that ACL entries can be defined. (31618249)
* Add a new batch error code for agents that fail during syncing. (31576140)
