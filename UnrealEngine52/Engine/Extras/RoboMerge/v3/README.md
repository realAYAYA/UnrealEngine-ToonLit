## Robomerge help and quick reference
<!-- The 'toc' comment is required by the marked-toc plugin and for roboserver.ts to insert a table of contents. -->
<!-- toc -->

### Intro
RoboMerge monitors commits to Perforce streams. It reads commit messages to find commands telling it to merge between branches, and some branches are set up to be automatically merged.

### Conflicts
If Perforce encounters a conflict when RoboMerge is merging a changelist, an email and Slack DM is sent to the author of the change, and also a Slack message is sent to the channel for the project.

There are three main ways to resolve a conflict, available on the web page and via buttons on the conflict email and Slack message:

#### Stomp

Available if the conflicting files are all assets. Use with care, as this will overwrite all changes in the target stream with those from the source.

When you select Stomp, RoboMerge will validate that the stomp is possible and, if it is, show a web page where you can confirm it should proceed with the stomp.

#### Create shelf

When you select create shelf, RoboMerge will open a web page to ask which workspace to create the Perforce shelf in. Shelf creation can take a few minutes.

Resolve the conflict in P4V as follows:

- unshelve the files from the shelf RoboMerge created
- resolve any conflicting files
- commit (in the commit dialog, press the button to delete the shelved files)

#### Skip

This tells RoboMerge to ignore the change and carry on. This is not available in all streams, because it can be hard to track down when changes were skipped when they shouldn't have been.


RoboMerge will perform no further merges on the stream until the conflict has been resolved. The web page shows which streams RoboMerge is currently blocked on, including the author responsible for resolving the conflicts.


### Example commands

![Example Robomerge Graph](public/images/RM-Example-Graph.png)

A common pattern is to configure RoboMerge to merge commits to Main into release and feature branches. In the above example, commits to **Release-5.0** are _automatically_ merged up to **Release-6.0**, and then up to **Main**. The merge paths for each of our development streams can be seen on the [RoboMerge web page](/).

To request a changelist committed to Main to be merged down to **Release-6.0** and **Release-5.0**, the changelist description would include the line:

`#robomerge Release-6.0, Release-5.0`

Multiple branches can be separated by commas and/or spaces. Most branches are given short aliases, which can also be found on the web page. This command could be simply written:

`#robomerge 5.0`

using the 5.0 alias and allowing RoboMerge to calculate the route from Main to 5.0. On the web page, hover over the box for a stream to see what aliases have been set up.

### Changelist Description Flags
**The following flags can be added to RoboMerge commands inside your Perforce commit description:**

| Symbol  | Name | Kind   | Example                   | Description                                                                                                                                                          |
|---------|------|--------|---------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| -       | skip | branch | `#robomerge -7.40`        | Do not merge to the specified branch (used for branches that are merged to automatically)                                                                            |
| ignore  |      | global | `#robomerge ignore`       | Completely ignore this changelist - no automatic merges happen. Note that a target branch of 'ignore' is interpreted as an ignore flag (can be used without a #).    |
| deadend |      | global | `#robomerge deadend`      | Completely ignore this changelist - no automatic merges happen. Note that a target branch of 'deadend' is interpreted as a deadend flag (can be used without a #).   |
| #manual |      | global | `#robomerge #manual`      | Do not commit merge - add #codereview and shelve for owner                                                                                                           |
| !       | null | branch | `#robomerge !7.40`        | Perform a null merge to the specified branch, i.e. convince Perforce that a merge has happened, but don't actually make any changes to the content of the stream.    |
| null    |      | global | `#robomerge null`         | Make every automatic merge of this commit a null merge (this is the old behavior of the deadend tag). Like ignore and deadend, can be used without a #.              |

Note that `#robomerge none` is now an error, due to confusion surrounding its usage.

#### Example cases
Most of the changes are fine but one file should be deadended:

Two possibilities:

- Accept target on that file
- Exclude the file from the shelf, check in, and hit ignore on that one file (this may cause a problem for subsequent check ins that modify this file)

#### Bot config

Unless noted otherwise, edge properties can be specied on source nodes and will
apply to all outgoing edges (not allowed in default node properties). No check
mark in edge columns means the setting can currently *only* be specified on
source nodes. 

No default means null/empty string/empty list.

Question remains about whether list properties should be cumulative if specified
for both node and edge.


|name                      |used by|default|edge?|description                           |notes|
|--------------------------|:-----:|-------|-----|--------------------------------------|-----|
|`checkIntervalSecs`        |b |`30`        | |All edges are round-robinned within this time, unless delayed by integrations| |
|`noStreamAliases`          |b |`false`     | |Stream names not available for commands, e.g. if there are duplicate stream names in bot | |
|`reportToBuildHealth`      |b |`false`     | |UGS integration                          | |
|`slackChannel`             |b |            | |Channel to receive blockages             |Not practical to make this per edge|
|`visibility`               |bn|`['fte']`   | |Permissions to access bot/node on web page| |
|`aliases`                  |n |            | |Alternative names for use in commands    | |
|`badgeProject`             |n |            | |UGS integration                          | |
|`depot`                    |n |**required**| |Depot, e.g. UE4                          |Flag currently called `defaultStreamDepot`|
|`rootPath`                 |n |from stream | |P4 depot full path                       | |
|`streamSubpath`            |n |`/...`      | |P4 depot sub-path                        | |
|`workspaceNameOverride`    |n |            | |Used specified workspace name for commits| |
|`enabled`                  |ne|`true`      | |If false, pretends node/edge doesn't exist| |
|`forcePause`               |ne|            | |If flag set, pause - applies each restart| |
|`additionalSlackChannelForBlockages` |e |  | |Single extra Slack channel               | |
|`blockAssetFlow`           |n |`[]`        | |Reject any integrations containing assets to these targets   | |
|`defaultIntegrationMethod` |e |normal      | |For edigrate                             | |
|`disallowSkip`             |e |            | |Remove skip option from UI               | |
|`emailOnBlockage`          |e |`true`      | |Email owners of conflicts?               | |
|`excludeAuthors`           |e |            | |Skip changes by these authors, e.g. skip `buildmachine` commits| |
|`ignoreBranchspecs`        |e |            | |Where branchspecs specified, ignore for this edge| |
|`incognitoMode`            |e |`false`     | |Terse description in committed changelists| |
|`initialCL`                |e |            | |First run only: which CL start _after_   | |
|`isDefaultBot`             |e |`false`     | |Run plain #robomerge commands? Should be `false` for streams monitored by multiple bots| |
|`lastGoodCLPath`           |e |            | |'Gate' file to read to find CIS-approved CL| |
|`maxFilesPerIntegration`   |e |`-1`        | |Reject integrations with more files than this| |
|`notify`                   |e |            | |Additional people to email on blockages  |Also `globalNotify`|
|`resolver`                 |e |            | |Single designated resolver               |Currently applies to both source and target nodes|

##### Integration window examples

###### Pause CIS when catching up to a gate

```
{ "pauseCISUnlessAtGate": true }
```

###### Specify integration windows when a gate catch-up may begin

Allow integration between 7pm and 5am UTC

```
{ "integrationWindow": [{ "startHourUTC": 19, "durationHours": 10 }] }
```

Allow integration 2am -> 5am and 9pm -> midnight UTC

```
{ "integrationWindow": [{ "startHourUTC": 2, "durationHours": 3 }, { "startHourUTC": 21, "durationHours": 3 }] }
```

Dis-allow integration between 5am and 9pm UTC

```
{ "integrationWindow": [{ "startHourUTC": 5, "durationHours": 16 }], "invertIntegrationWindow": true }
```
