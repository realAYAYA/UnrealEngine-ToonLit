# Horde.Storage

Horde.Storage - A Distributed storage service for use with Unreal Engine

# Components 

* Horde.Storage - Main service - The service implementing most of the logic
* Callisto - Transaction - A C# service that manages and serializes the ref transactions from the ref server. Essentially a custom db for Horde.Storage focusing on a log structured approch to the transaction rather then the traditional db aspects of storage. This is a legacy component that is being faced out.
* Jupiter.Common - Lib - Library with common shared infrastructure that is reused across all or some C# services


# Chart Values

## Globals

| Name | Description | Possible Values |
| ------------- |:-------------:| -----:|
| cloudProvider | Which cloud provider this will be deployed on, impacts which services we interact with. | AWS or Azure |
| awsRegion | Only used for AWS cloud, which region you expect the services to be in | AWS Region format |
| awsRoleArn | The arn of a AWS role to assume | A valid ARN | 
| siteName | The name of this particular site within your cluster of Jupiter nodes. Should be unique amongst all your jupiter sites and is used to understand were a record has been replicated | Any globally unique value |
| authMethod | Which method is used to auth | Okta / JWTBearer / Disabled |
| oktaDomain | The okta domain used to auth against | See your okta setup |
| oktaAuthorizationServerId | The authorization server id in okta | See your okta setup for which values are valid |
| jwtAudience | The audience expected in your JWT token, used both with Okta and JWTBearer | The JWT audience
| jwtAuthority | The address to your JWT Authority, only used for JWT Bearer verification | A url to your JWT authority |


