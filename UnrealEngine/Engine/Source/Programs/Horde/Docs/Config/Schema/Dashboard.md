[Horde](../../../README.md) > [Configuration](../../Config.md) > *.dashboard.json

# *.dashboard.json

Configuration for dashboard features

Name | Description
---- | -----------
`showLandingPage` | `boolean`<br>Navigate to the landing page by default
`showCI` | `boolean`<br>Enable CI functionality
`showAgents` | `boolean`<br>Whether to show functionality related to agents, pools, and utilization on the dashboard.
`showAgentRegistration` | `boolean`<br>Whether to show the agent registration page. When using registration tokens from elsewhere this is not needed.
`showPerforceServers` | `boolean`<br>Show the Perforce server option on the server menu
`showDeviceManager` | `boolean`<br>Show the device manager on the server menu
`showTests` | `boolean`<br>Show automated tests on the server menu
`agentCategories` | [`DashboardAgentCategoryConfig`](#dashboardagentcategoryconfig)`[]`<br>Configuration for different agent pages
`poolCategories` | [`DashboardPoolCategoryConfig`](#dashboardpoolcategoryconfig)`[]`<br>Configuration for different pool pages
`analytics` | [`TelemetryViewConfig`](#telemetryviewconfig)`[]`<br>Configuration for telemetry views
`include` | [`ConfigInclude`](#configinclude)`[]`<br>Includes for other configuration files
`macros` | [`ConfigMacro`](#configmacro)`[]`<br>Macros within this configuration

## DashboardAgentCategoryConfig

Configuration for a category of agents

Name | Description
---- | -----------
`name` | `string`<br>Name of the category
`condition` | `string`<br>Condition string to be evaluated for this page

## DashboardPoolCategoryConfig

Configuration for a category of pools

Name | Description
---- | -----------
`name` | `string`<br>Name of the category
`condition` | `string`<br>Condition string to be evaluated for this page

## TelemetryViewConfig

A telemetry view of related metrics, divided into categofies

Name | Description
---- | -----------
`id` | `string`<br>Identifier for the view
`name` | `string`<br>The name of the view
`telemetryStoreId` | `string`<br>The telemetry store this view uses
`variables` | [`TelemetryVariableConfig`](#telemetryvariableconfig)`[]`<br>The variables used to filter the view data
`categories` | [`TelemetryCategoryConfig`](#telemetrycategoryconfig)`[]`<br>The categories contained within the view

## TelemetryVariableConfig

A telemetry view variable used for filtering the charting data

Name | Description
---- | -----------
`name` | `string`<br>The name of the variable for display purposes
`group` | `string`<br>The associated data group attached to the variable
`defaults` | `string[]`<br>The default values to select

## TelemetryCategoryConfig

A chart categody, will be displayed on the dashbord under an associated pivot

Name | Description
---- | -----------
`name` | `string`<br>The name of the category
`charts` | [`TelemetryChartConfig`](#telemetrychartconfig)`[]`<br>The charts contained within the category

## TelemetryChartConfig

Telemetry chart configuraton

Name | Description
---- | -----------
`name` | `string`<br>The name of the chart, will be displayed on the dashboard
`display` | [`TelemetryMetricUnitType`](#telemetrymetricunittype-enum)<br>The unit to display
`graph` | [`TelemetryMetricGraphType`](#telemetrymetricgraphtype-enum)<br>The graph type
`metrics` | [`TelemetryChartMetricConfig`](#telemetrychartmetricconfig)`[]`<br>List of configured metrics
`min` | `integer`<br>The min unit value for clamping chart
`max` | `integer`<br>The max unit value for clamping chart

## TelemetryMetricUnitType (Enum)

The units used to present the telemetry

Name | Description
---- | -----------
`Time` | Time duration
`Ratio` | Ratio 0-100%
`Value` | Artbitrary numeric value

## TelemetryMetricGraphType (Enum)

The type of

Name | Description
---- | -----------
`Line` | A line graph
`Indicator` | Key performance indicator (KPI) chart with thrasholds

## TelemetryChartMetricConfig

Metric attached to a telemetry chart

Name | Description
---- | -----------
`id` | `string`<br>Associated metric id
`threshold` | `integer`<br>The threshold for KPI values
`alias` | `string`<br>The metric alias for display purposes

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
