# Horde.Storage

Horde.Storage - A Distributed storage service

# Directories 

* Horde.Storage - Most of Horde.Storage functionality.
* Callisto - Transaction - A legacy C# service that manages and serializes the references inserted into Horde.Storage. Essentially a custom db for Horde.Storage focusing on a log structured approch to the transaction rather then the traditional db aspects of storage.
* Jupiter.Common - Lib - Library with common shared infrastructure that is reused across Horde.Storage and Callisto. 
* Helm - Helm charters for all components
* Benchmark - Useful templates for doing simple http benchmarking user SuperBenchmark (sb)
* Composes - a bunch of different docker compose files for easier local deployment and testing

# Dependencies

* DotNet Core 5 (and Visual Studio 2019 or VS Code)
* Docker

# Other useful things
* MongoDB
* Scylla
* Minio
* Docker Compose

# Functional Test requirements

Before starting the tests start the prerequite services using this command line
`docker-compose -f Composes\docker-compose-tests.yml -p jupiter-test up`


# Running locally
If you want to try out Horde.Storage you can easily start it up using docker-compose. Note that as Horde.Storage supports multiple different backends we provide different compositions for your use-case.
To start run:
`docker-compose -f Composes\docker-compose.yml -f Composes\docker-compose-aws.yml up --build`
The AWS compose file can be replaced with `docker-compose-azure.yml` if you want to run with services closer to what is available on Azure.

Docker compose setups disable authentication for to make it quick to get started, generally we recommend that you hook Horde.Storage up to a OIDC provider before deploying this.

Horde.Storage hosts Swagger documentation at `/docs` (so `http://localhost/docs` when running locally). This page lets you pick the service you want to call and lists the API for it.
Note that some endpoints using binary protocol for effieceny and the format of those is not documented in swagger, we always provide a REST compatible api for every use case.

# Deployment
Horde.Storage is currently only run in deployment on AWS, but the requirements on storage and db are very generic and also abstracted. 
As such Horde.Storage supports running against local storage and Mongo for a small scale fully on premise deployment. Simlarly we have basic (untested) support for Azure services.

We provide helm values (under `/Helm`) that we use for epic internal deployments. For prod deployment use the `base` values as well as the value file for the region you are deploying to.

## AWS
This is the most tested deployment form as this is how we operate it at Epic. We manage the AWS deployments thru a Terraform script, the key requirement is setting up the replicated Dynamodb tables in the regions we want to run Horde.Storage in.
The helm chart we install into each regions kubernetes cluster is provided in this repo.

## On premise
Horde.Storage can be deployed onprem without using any cloud resources but it does require a kubernetes cluster to run in for production cases. You will also need to setup a Mongo database for it to run against.
Note that if you intend to deployment to multiple on prem sites you will need to manually setup replication of the database used.

# Monitoring
We use Datadog to monitor our services, as such Horde.Storage is instrumented to work well with that service. But all logs are delivered as structured logs to stdout, so any monitoring service that understands structured logs should be able to monitor it quite well.

## Health Checks
All Horde.Storage services use health checks to monitor themselves, any background services they may run and any dependent service they may have (DB / Blob store etc).
You can reach the health checks at `/health/live` and `/health/ready` for live and ready checks respectively. Ready checks is used to verify that the service is working, if this returns false the app will not get any traffic (load balancer ignores it). Live checks are used to see if the pod is working as it should, if this returns false the entire pod is killed. This only applies when run in a kubernetes cluster.

# Authentication
Horde.Storage supports using any OIDC provider that does JWT verfication for authentication. We use Okta at Epic so this is what has been tested but other OIDCs should be compatible as well.

You configure authentication in two steps, setting up the IdentityProvider (IdP) and then setting up authorization for each namespace.

## IdentityProvider setup

You specify auth schemes to in the `auth` settings. 
```  
auth:
    defaultScheme: Bearer
    schemes:
      Bearer: 
        implementation: "Jwt"
        jwtAudience: "api://horde"
        jwtAuthority: "<url-to-your-idp>
```
We recommend naming your scheme `Bearer` if its your first and only scheme. You can use multiple schemes to connect against multiple IdPs, this is mostly useful during a migration.


## Namespace access

Access to operations within Horde Storage is controlled using a set of actions:
```
    - ReadObject
    - WriteObject
    - DeleteObject
    - DeleteBucket
    - DeleteNamespace
    - ReadTransactionLog
    - WriteTransactionLog
    - AdminAction
```

These can be assigned either per namespace using the acls in each namespace policy or by assigning them to the acls in the Auth settings (which applies them across all namespaces and for operations not associated with a namespace)
Example configuration that sets transaction log access for users that have access to it, admin access for admins and then per namespace access.
```
auth:
    acls:
    - claims: 
        - groups=app-horde-storage-transactionlog
        actions:
        - ReadTransactionLog
        - WriteTransactionLog

    - claims: 
        - groups=app-horde-storage-admin
        actions:
        - ReadObject
        - WriteObject
        - DeleteObject
        - DeleteBucket
        - DeleteNamespace
        - AdminAction

namespace:
  policy:
    example-namespace:
      acls:
      - actions: 
        - ReadObject
        - WriteObject
        claims: 
        - ExampleClaim
     - actions: 
        - ReadObject
        - WriteObject
        claims: 
        - AnotherClaim
    open-namespace:
      acls:
      - actions: 
        - ReadObject
        - WriteObject
        claims: 
        - "*"
```

If you specify multiple claims in the claims array these are ANDed together thus requiring all the claims. A claim statement like `A=B` requires claim `A` to have value `B` (or contain value `B` in the case of a array).
You can also specify the `*` claim which grants access for any valid token no matter which claims it has, this is mostly for debug / testing scenarios and shouldnt be used for production data.

# Development
## Debugging profiles
We have two seperate Debug profiles `Local deployment` and `IIS Express`. `ISS Express` configures the service to with in memory mocks of dependent services, allowing for quick iteration but limited functionality.
`Local Deployment` starts the service on a dedicated port, with configuration to reach other locally running services on their dedicated port. Can be useful to debug and run Horde.Storage without any virtualization. Note that it can be a bit of work to setup all dependencies locally for Local Deployment. If you just want to test things, or step thru the flow we recommend using the docker compose setup and attaching to that.

## Making a new release

Update appversion in version.yaml
Update changelog.md changing the unreleased section into the version number of the new release.
Push the version as a git tag - convert this into a release on Github.