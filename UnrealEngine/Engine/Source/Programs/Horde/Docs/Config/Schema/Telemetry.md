[Horde](../../../README.md) > [Configuration](../../Config.md) > *.telemetry.json

# *.telemetry.json

Config for metrics

Name | Description
---- | -----------
`id` | `string`<br>Identifier for this store
`acl` | [`AclConfig`](#aclconfig)<br>Permissions for this store
`metrics` | [`MetricConfig`](#metricconfig)`[]`<br>Metrics to aggregate on the Horde server
`include` | [`ConfigInclude`](#configinclude)`[]`<br>Includes for other configuration files
`macros` | [`ConfigMacro`](#configmacro)`[]`<br>Macros within this configuration

## AclConfig

Parameters to update an ACL

Name | Description
---- | -----------
`entries` | [`AclEntryConfig`](#aclentryconfig)`[]`<br>Entries to replace the existing ACL
`profiles` | [`AclProfileConfig`](#aclprofileconfig)`[]`<br>Defines profiles which allow grouping sets of actions into named collections
`inherit` | `boolean`<br>Whether to inherit permissions from the parent ACL
`exceptions` | `string[]`<br>List of exceptions to the inherited setting

## AclEntryConfig

Individual entry in an ACL

Name | Description
---- | -----------
`claim` | [`AclClaimConfig`](#aclclaimconfig)<br>Name of the user or group
`actions` | `string[]`<br>Array of actions to allow
`profiles` | `string[]`<br>List of profiles to grant

## AclClaimConfig

New claim to create

Name | Description
---- | -----------
`type` | `string`<br>The claim type
`value` | `string`<br>The claim value

## AclProfileConfig

Configuration for an ACL profile. This defines a preset group of actions which can be given to a user via an ACL entry.

Name | Description
---- | -----------
`id` | `string`<br>Identifier for this profile
`actions` | `string[]`<br>Actions to include
`excludeActions` | `string[]`<br>Actions to exclude from the inherited actions
`extends` | `string[]`<br>Other profiles to extend from

## MetricConfig

Configures a metric to aggregate on the server

Name | Description
---- | -----------
`id` | `string`<br>Identifier for this metric
`filter` | `string`<br>Filter expression to evaluate to determine which events to include. This query is evaluated against an array.
`property` | `string`<br>Property to aggregate
`groupBy` | `string`<br>Property to group by. Specified as a comma-separated list of JSON path expressions.
`function` | [`AggregationFunction`](#aggregationfunction-enum)<br>How to aggregate samples for this metric
`percentile` | `integer`<br>For the percentile function, specifies the percentile to measure
`interval` | `string`<br>Interval for each metric. Supports times such as "2d", "1h", "1h30m", "20s".

## AggregationFunction (Enum)

Method for aggregating samples into a metric

Name | Description
---- | -----------
`Count` | Count the number of matching elements
`Min` | Take the minimum value of all samples
`Max` | Take the maximum value of all samples
`Sum` | Sum all the reported values
`Average` | Average all the samples
`Percentile` | Estimates the value at a certain percentile

## ConfigInclude

Directive to merge config data from another source

Name | Description
---- | -----------
`path` | `string`<br>Path to the config data to be included. May be relative to the including file's location.

## ConfigMacro

Declares a config macro

Name | Description
---- | -----------
`name` | `string`<br>Name of the macro property
`value` | `string`<br>Value for the macro property
