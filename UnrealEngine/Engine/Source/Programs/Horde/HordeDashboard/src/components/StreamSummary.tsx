// Copyright Epic Games, Inc. All Rights Reserved.

import { DefaultButton, DetailsList, DetailsListLayoutMode, DetailsRow, FocusZone, FocusZoneDirection, IColumn, IDetailsGroupRenderProps, IDetailsListProps, mergeStyleSets, Pivot, PivotItem, ScrollablePane, ScrollbarVisibility, SelectionMode, Spinner, SpinnerSize, Stack, Text } from '@fluentui/react';
import { action, observable } from 'mobx';
import { observer } from 'mobx-react-lite';
import moment from 'moment-timezone';
import React, { useEffect, useState } from 'react';
import { Link, useLocation, useParams } from 'react-router-dom';
import backend, { useBackend } from '../backend';
import { FindIssueResponse, GetExternalIssueResponse, JobsTabData, TabType } from '../backend/Api';
import dashboard from '../backend/Dashboard';
import { ProjectStore } from '../backend/ProjectStore';
import { getElapsedString, getShortNiceTime } from '../base/utilities/timeUtils';
import { detailClasses, hordeClasses } from '../styles/Styles';
import { IssueModalV2 } from './IssueViewV2';
import { useQuery } from './JobDetailCommon';
import { SchedulePane } from './SchedulePane';
import { IssueStatusIconV2 } from './StatusIcon';
import { BuildHealthTestReportPanel } from './TestReportPanel';


class SummaryHandler {

   set(streamId: string) {

      if (this.streamId === streamId) {
         return;
      }

      this.streamId = streamId;
      this.poll();

   }

   clear() {

      for (let i = 0; i < this.cancelID; i++) {
         this.canceled.add(i);
      }

      clearTimeout(this.timeoutID);

      this.updating = false;
      this.streamId = undefined;
      this.issues = [];
      this.unpromoted = [];

      this.collapsedIssueGroups = new Map();

      this.intialLoad = true;
   }

   private async poll() {

      try {

         if (!this.streamId || this.updating) {
            return;
         }

         clearTimeout(this.timeoutID);
         if (this.updateMS) {
            this.timeoutID = setTimeout(() => { this.poll(); }, this.updateMS);
         }

         this.updating = true;
         const cancelID = this.cancelID++;

         const values = await Promise.all([backend.getIssuesV2({ streamId: this.streamId, count: 512, resolved: false })]);

         let jiraKeys: string[] = [];
         
         const jiraIssues: Set<number> = new Set();
         values[0].forEach(issue => {            
            if (issue.externalIssueKey) {
               jiraIssues.add(issue.id)
               jiraKeys.push(issue.externalIssueKey);
            }
         });

         const now = moment(Date.now());
         jiraKeys = jiraKeys.filter(j => {
            const issue = this.jiraIssues.get(j);
            if (!issue) {
               return true;
            }
            if (moment.duration(now.diff(issue.cacheTime)).asMinutes() <= 2) {
               return false;
            }
            return true;
         });

         if (jiraKeys.length) {
            const jiras = await backend.getExternalIssues(this.streamId, jiraKeys);
            jiras.forEach(j => {
               this.jiraIssues.set(j.key, { issue: j, cacheTime: moment(Date.now()) });
            })
         }


         this.issues = values[0].filter(p => p.promoted || jiraIssues.has(p.id));
         this.unpromoted = values[0].filter(p => !p.promoted && !jiraIssues.has(p.id));

         if (this.canceled.has(cancelID)) {
            return;
         }

         this.intialLoad = false;
         this.updated();

      } catch (reason) {
         console.error(reason);
      } finally {
         this.updating = false;
      }

   }

   exportIssues(projectStore: ProjectStore, items: HealthItem[], groups: IssueGroup[], promoted: boolean) {

      const stream = projectStore.streamById(this.streamId);

      if (!stream || !stream.project) {
         return;
      }

      const timeStr = moment().format('YYYY-MM-DD');

      const name = `++${stream.project.name}+${stream.name}-BuildHealth-${promoted ? "Promoted" : "Current"}-${timeStr}.html`;

      // build issue table
      let html = `<!DOCTYPE html>
        <html>
        <head>
        <style>
		body {
			max-width: 1480px;
			margin: auto;
		}
        table {
          font-family: arial, sans-serif;
          font-size: 12px;
          border-collapse: collapse;
          width: 100%;
        }
        
        td, th {
          border: 1px solid #dddddd;
          text-align: center;
          padding: 8px;
        }
        
        tr:nth-child(odd) {
          background-color: #dedddc;
        }
        </style>
        </head>        
        <body>`;

      html += `<h2>Build Health - ${stream.fullname} - ${promoted ? "Promoted" : "Current"} - ${timeStr}</h2>`;

      html += `<table>`;

      html += `<tr>
        <td>Category</td>
        <td>Issue</td>
        <td>Summary</td>
        <td>Status</td>
        <td>Open Since</td>
        <td>Promoted</td>
        <td>Jira</td>
      </tr>`

      groups.forEach(g => {


         for (let i = g.startIndex; i < g.startIndex + g.count; i++) {

            const item = items[i];

            const issue = item.issue;

            let status = "Unassigned";
            if (issue.owner?.name) {
               status = issue.owner?.name;
            }

            if (issue.resolvedAt) {
               status = "Resolved";
               if (issue.fixChange) {
                  status += ` in CL ${issue.fixChange}`;
               }
            }

            let summary = issue.summary;
            if (summary.length > 100) {
               summary = summary.slice(0, 100) + "...";
            }

            const openSince = `${getShortNiceTime(issue.createdAt)} (${getElapsedString(moment(issue.createdAt), moment.utc(), false).trim()})`;

            const href = `${window.location.protocol}//${window.location.hostname}${window.location.pathname}?tab=summary&issue=${issue.id}`;

            let jiraTag = "";

            if (issue.externalIssueKey) {      
               
               const url = `${dashboard.externalIssueService?.url}/browse/${issue.externalIssueKey}`;      
               jiraTag = `<a href="${url}" target="_blank">${issue.externalIssueKey}</a>`;
            }


            html += `<tr>
                <td>${g.name}</td>
                <td><a href="${href}" target="_blank">${issue.id}</a></td>
                <td style="text-align:left"><a href="${href}" target="_blank">${summary}</a></td>
                <td>${status}</td>
                <td>${openSince}</td>
                <td>${issue.promoted ? "Yes" : "No"}</td>
                <td>${jiraTag}</td>
              </tr>`;

         }

      });

      html += `</table>`;

      // close html
      html += `</body>
        </html>`;

      const element = document.createElement('a');
      element.setAttribute('href', 'data:text/plain;charset=utf-8,' + encodeURIComponent(html));
      element.setAttribute('download', name);

      element.style.display = 'none';
      document.body.appendChild(element);

      element.click();

      document.body.removeChild(element);
   }

   @observable
   update = 0


   @action
   private updated() {
      this.update++;
   }

   intialLoad = true;

   issues: FindIssueResponse[] = [];
   unpromoted: FindIssueResponse[] = [];

   collapsedIssueGroups: Map<string, boolean> = new Map();

   streamId?: string;

   private updating = false;

   private updateMS = 30000;
   private timeoutID?: any;
   private canceled = new Set<number>();
   private cancelID = 0;

   jiraIssues: Map<string, { issue: GetExternalIssueResponse, cacheTime: moment.Moment }> = new Map();

}


const handler = new SummaryHandler();

type HealthItem = {
   issue: FindIssueResponse;
};

type IssueGroup = {
   count: number;
   key: string;
   name: string;
   headerText: string;
   startIndex: number;
   level?: number;
   isCollapsed?: boolean;
}

const sortIssueGroups = (itemsIn: HealthItem[], projectStore: ProjectStore): [HealthItem[], IssueGroup[]] => {

   const stream = projectStore.streamById(handler.streamId)!;

   const foundIssues = new Set<number>();

   const tabIssues = new Map<string, HealthItem[]>();

   const groups: IssueGroup[] = [];

   let curIndex = 0;

   stream?.tabs?.forEach(tab => {

      if (tab.type !== TabType.Jobs) {
         return;
      }

      const jobTab = tab as JobsTabData;

      itemsIn.forEach(item => {

         const issue = item.issue;

         const templates = new Set(issue.spans.map(s => s.templateId));

         if (!(jobTab).templates?.find((tid) => templates.has(tid))) {
            return;
         }


         // only show on first tab
         if (foundIssues.has(issue.id)) {
            return;
         }

         foundIssues.add(issue.id);

         if (!tabIssues.has(tab.title)) {

            const key = `issue_group_${tab.title}`;

            groups.push({
               key: key,
               name: tab.title,
               count: 1,
               headerText: tab.title,
               startIndex: curIndex,
               isCollapsed: !handler.collapsedIssueGroups.get(key)
            });

            tabIssues.set(tab.title, []);
         }

         tabIssues.get(tab.title)!.push({
            issue: issue
         })

         curIndex++;
      })

   })

   const name = "Uncategorized";
   const uncategorized = itemsIn.filter(item => !foundIssues.has(item.issue.id));
   if (uncategorized.length) {
      const key = `issue_group_${name}`;
      groups.push({
         key: key,
         name: name,
         count: 1,
         headerText: name,
         startIndex: curIndex,
         isCollapsed: !handler.collapsedIssueGroups.get(key)
      });

      tabIssues.set(name, []);

      uncategorized.forEach(item => {

         tabIssues.get(name)!.push({
            issue: item.issue
         })
         curIndex++;
      })
   }

   const allIssues: HealthItem[] = [];
   tabIssues.forEach((items, name) => {

      const group = groups.find(g => g.name === name)!
      group.count = items.length;

      allIssues.push(...items.sort((a, b) => b.issue.id - a.issue.id));

   });

   return [allIssues, groups];

}

const HealthPanelIssues: React.FC<{ desktopAlerts?: boolean }> = observer(({ desktopAlerts }) => {

   const query = useQuery();
   const location = useLocation();
   const { projectStore } = useBackend();
   const [issueHistory, setIssueHistory] = useState(false);
   const [currentPivot, setCurrentPivot] = useState("$promoted");

   // subscribe
   if (handler.update) { }

   const allIssues = handler.issues.concat(handler.unpromoted);

   let issues: FindIssueResponse[] = [];

   if (currentPivot === "$promoted") {
      issues = handler.issues;
   }

   if (currentPivot === "$current") {
      issues = handler.unpromoted;
   }

   const stream = projectStore.streamById(handler.streamId);   
   if (stream) {
      const config = stream.workflows?.find(w => w.id === currentPivot);
      if (config) {
         issues = allIssues.filter(issue => {
            return issue.openWorkflows.indexOf(currentPivot) !== -1;            
         });
      }
   }

   const [items, groups] = sortIssueGroups(issues.map(i => { return { issue: i } }), projectStore);

   // 508
   const columns = [
      { key: 'health_column1', name: 'Summary', minWidth: 690, maxWidth: 690, isResizable: false },
      { key: 'health_column2', name: 'Quarantine', minWidth: 80, maxWidth: 80, isResizable: false },
      { key: 'health_column3', name: 'Jira', minWidth: 80, maxWidth: 80, isResizable: false },
      { key: 'health_column4', name: 'JiraPriority', minWidth: 64, maxWidth: 64, isResizable: false },
      { key: 'health_column5', name: 'JiraAssignee', minWidth: 240, maxWidth: 240, isResizable: false },
      { key: 'health_column6', name: 'JiraStatus', minWidth: 64, maxWidth: 64, isResizable: false },
      { key: 'health_column7', name: 'Status', minWidth: 160, maxWidth: 160, isResizable: false },
      { key: 'health_column8', name: 'Opened', minWidth: 150, maxWidth: 150, isResizable: false },

   ];

   const renderItem = (item: HealthItem, index?: number, column?: IColumn) => {

      if (!column) {
         return <div />;
      }

      const issue = item.issue;

      const textSize = "small";

      let ack = issue.acknowledgedAt ? "" : "Pending";
      let status = "Unassigned";
      if (issue.owner?.name) {
         status = issue.owner?.name;
      }
      if (issue.resolvedAt) {
         status = "Resolved";
         if (issue.fixChange) {
            status += ` in CL ${issue.fixChange}`;
         }
      }

      if (status === "Unassigned" || issue.resolvedAt) {
         ack = "";
      }

      if (column.name === "Summary") {

         let summary = issue.summary;
         if (summary.length > 100) {
            summary = summary.slice(0, 100) + "...";
         }

         return <Stack horizontal disableShrink={true}>{<IssueStatusIconV2 issue={issue} streamId={handler.streamId} />}<Text variant={textSize}>{`Issue ${issue.id} - ${summary}`}</Text></Stack>;
      }

      if (column.name === "Quarantine") {

         if (!issue.quarantinedBy) {
            return null;
         }

         return <Stack horizontalAlign="start" disableShrink={true} verticalAlign="center" ><Text variant={textSize}>Quarantined</Text></Stack>;
      }


      if (column.name === "Status") {

         if (ack) {
            status += ` (${ack})`;
         }

         return <Stack horizontalAlign="end" disableShrink={true} verticalAlign="center"><Text variant={textSize}>{status}</Text></Stack>;
      }

      if (issue.externalIssueKey) {

         const jissue = handler.jiraIssues.get(issue.externalIssueKey)?.issue;

         const textDecoration = jissue?.resolutionName ? "line-through" : undefined;

         if (column.name === "JiraPriority") {

            if (!jissue || !jissue.priorityName) {
               return null;
            }

            let priority = jissue.priorityName;
            if (priority.indexOf("- ") !== -1) {
               priority = priority.slice(priority.indexOf("- ") + 2);
            }

            return <Stack horizontalAlign="start" disableShrink={true}><a href={jissue.link} target="_blank" rel="noreferrer" onClick={(ev) => ev.stopPropagation()}><Text style={{ textDecoration: textDecoration }} variant={textSize}>{priority}</Text> </a></Stack>;

         }


         if (column.name === "JiraAssignee") {

            if (!jissue) {
               return null;
            }

            return <Stack horizontalAlign="center" disableShrink={true}><a href={jissue.link} target="_blank" rel="noreferrer" onClick={(ev) => ev.stopPropagation()}><Text style={{ textDecoration: textDecoration }} variant={textSize}>{jissue.assigneeDisplayName ?? "Unassigned (Jira)"}</Text> </a></Stack>;
         }

         if (column.name === "JiraStatus") {

            const desc = jissue?.resolutionName ?? jissue?.statusName;

            if (!jissue) {
               return null;
            }

            return <Stack horizontalAlign="start" disableShrink={true}><a href={jissue.link} target="_blank" rel="noreferrer" onClick={(ev) => ev.stopPropagation()}><Text style={{ textDecoration: textDecoration }} variant={textSize}>{desc}</Text> </a></Stack>;
         }

         if (column.name === "Jira") {

            if (!jissue) {
               return null;
            }

            return <Stack horizontalAlign="start" disableShrink={true}><a href={jissue.link} target="_blank" rel="noreferrer" onClick={(ev) => ev.stopPropagation()}><Text style={{ textDecoration: textDecoration }} variant={textSize}>{jissue.key}</Text> </a></Stack>;
         }
      }


      if (column.name === "Ack") {
         return <Text>{ack}</Text>;
      }


      if (column.name === "Opened") {
         const openSince = `${getShortNiceTime(issue.createdAt)} (${getElapsedString(moment(issue.createdAt), moment.utc(), false).trim()})`;
         return <Stack style={{ paddingRight: 8 }} horizontalAlign="end" verticalAlign="center"><Text variant={textSize}>{openSince}</Text></Stack>;
      }

      return null;
   }

   const classes = mergeStyleSets({
      detailsRow: {
         selectors: {
            '.ms-DetailsRow-cell': {
               padding: 8,
               overflow: "hidden",
               whiteSpace: "nowrap"
            }
         }
      }
   });

   const renderRow: IDetailsListProps['onRenderRow'] = (props) => {

      if (props) {

         const item = props!.item as HealthItem;
         query.set("issue", item.issue.id.toString());
         const href = `${location.pathname}?${query.toString()}`;

         return <Link to={href} className="job-item" onClick={() => setIssueHistory(true)}>
            <DetailsRow className={classes.detailsRow} {...props} />
         </Link>;

      }
      return null;
   };


   const onRenderGroupHeader: IDetailsGroupRenderProps['onRenderHeader'] = (props) => {
      if (props) {

         const group = props.group! as IssueGroup;

         return (
            <div className="view-log-link">
               <div className={detailClasses.headerAndFooter} style={{ marginRight: 8, padding: 2 }} onClick={() => {

                  handler.collapsedIssueGroups.set(group.key, !handler.collapsedIssueGroups.get(group.key));
                  props.onToggleCollapse!(props.group!);
               }}>
                  <div style={{
                     fontSize: "13px",
                     padding: '4px 8px',
                     userSelect: 'none',
                     color: "#404040",
                     fontFamily: "Horde Open Sans SemiBold"
                  }}>{`${group.headerText}`}</div>
               </div>
            </div>
         );
      }

      return null;
   };


   groups.forEach(g => g.headerText += ` (${g.count})`);

   const pivotItems: JSX.Element[] = [];

   let issueText = "Promoted";
   let triageText = "Current";
   if (handler.issues.length) {
      issueText += ` (${handler.issues.length})`
   }
   if (handler.unpromoted.length) {
      triageText += ` (${handler.unpromoted.length})`
   }

   pivotItems.push(<PivotItem headerText={issueText} itemKey={`issue_pivot_item_key_$promoted`} key={`issue_pivot_key_$promoted`} headerButtonProps={{ onClick: () => { setCurrentPivot("$promoted") } }} />);
   pivotItems.push(<PivotItem headerText={triageText} itemKey={`issue_pivot_item_key_$current`} key={`issue_pivot_key_$current`} headerButtonProps={{ onClick: () => { setCurrentPivot("$current") } }} />);

   stream?.workflows?.forEach(config => {

      const workflowIssues = allIssues.filter(issue => issue.openWorkflows.indexOf(config.id) !== -1);
      if (!workflowIssues.length) {
         return;
      }
      pivotItems.push(<PivotItem headerText={`${config.summaryTab ?? config.id} (${workflowIssues.length})`} itemKey={`issue_pivot_item_key_${config.id}`} key={`issue_pivot_key_${config.id}`} headerButtonProps={{ onClick: () => { setCurrentPivot(config.id) } }} />);
   });

   return <Stack style={{ width: 1720, marginLeft: 4 }}>
      <IssueModalV2 issueId={query.get("issue")} streamId={handler.streamId!} popHistoryOnClose={issueHistory} />
      <Stack styles={{ root: { paddingLeft: 4, paddingRight: 0, paddingTop: 4, paddingBottom: 4 } }}>
         <Stack>
            <Stack horizontal>
               <Stack horizontalAlign={"start"}>
                  <Pivot className={hordeClasses.pivot}
                     selectedKey={`issue_pivot_item_key_${currentPivot}`}
                     linkSize="normal"
                     linkFormat="links"
                     onLinkClick={(item => {
                        if (!item || !item.props.itemKey) {
                           return;
                        }
                        setCurrentPivot(item.props.itemKey.replace("issue_pivot_item_key_", ""));
                     })}>
                     {pivotItems}
                  </Pivot>
               </Stack>
               <Stack grow />
               <Stack style={{ paddingRight: 8 }}>
                  {(currentPivot === "$promoted" || currentPivot === "$current") && !handler.intialLoad && <DefaultButton text={`Export ${currentPivot === "$promoted" ? "Promoted" : "Current"}`} style={{ fontSize: 10, padding: 8, height: "24px", minWidth: "48px" }} onClick={() => handler.exportIssues(projectStore, items, groups, currentPivot === "$promoted")} />}
               </Stack>
            </Stack>
         </Stack>


         {!issues.length && !handler.intialLoad && <Stack style={{ paddingBottom: 12, paddingTop: 12 }} horizontal tokens={{ childrenGap: 12 }}><Text variant="medium">No issues found</Text></Stack>}
         {!issues.length && handler.intialLoad && <Stack style={{ paddingBottom: 12, paddingTop: 12 }} horizontal tokens={{ childrenGap: 12 }}><Text variant="medium">Loading issues</Text><Spinner size={SpinnerSize.medium} /></Stack>}
         {!!issues.length && <div style={{ overflowY: 'auto', overflowX: 'hidden', maxHeight: "640px" }} data-is-scrollable={true}>
            <Stack style={{ paddingTop: 6 }}>
               <DetailsList
                  compact={true}
                  isHeaderVisible={false}
                  indentWidth={0}
                  items={items}
                  groups={groups}
                  columns={columns}
                  setKey="set"
                  selectionMode={SelectionMode.none}
                  layoutMode={DetailsListLayoutMode.justified}
                  onRenderItemColumn={renderItem}
                  onRenderRow={renderRow}
                  groupProps={{
                     onRenderHeader: onRenderGroupHeader,
                  }}

               />
            </Stack>
         </div>}
      </Stack>
      <BuildHealthTestReportPanel streamId={handler.streamId!} />
   </Stack>

});

const HealthPanel: React.FC<{ desktopAlerts?: boolean }> = observer(({ desktopAlerts }) => {

   if (handler.update) { }

   return <Stack style={{ width: 1760, marginLeft: 4 }}>
      <Stack className={hordeClasses.raised}>
         <Stack tokens={{ childrenGap: 12 }}>
            <Stack>
               <Text variant="mediumPlus" styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>{desktopAlerts ? "Open Issues" : "Build Health"}</Text>
            </Stack>
            <Stack>
               <HealthPanelIssues desktopAlerts={desktopAlerts} />
            </Stack>
         </Stack>
      </Stack>
   </Stack>
});


const SchedulePanel: React.FC = observer(() => {

   const { projectStore } = useBackend();

   // subscribe
   if (handler.update) { }

   const stream = projectStore.streamById(handler.streamId!)!;

   let templates = stream.templates.filter(t => !!t.schedule).sort((a, b) => a.name < b.name ? -1 : 1);

   if (!templates.length) {
      return null;
   }

   return (<Stack style={{ width: 1760, marginLeft: 4 }}>
      <Stack className={hordeClasses.raised}>
         <Stack tokens={{ childrenGap: 12 }}>
            <Text variant="mediumPlus" styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>Schedule</Text>
            <Stack styles={{ root: { paddingLeft: 4, paddingRight: 0, paddingTop: 8, paddingBottom: 4 } }}>
               <SchedulePane templates={templates} />
            </Stack>
         </Stack>
      </Stack>
   </Stack>);
})


const StreamSummaryInner: React.FC = observer(() => {

   const { streamId } = useParams<{ streamId: string }>();

   if (!streamId) {
      return null;
   }

   // subscribe
   if (handler.update) { }

   return <Stack tokens={{ childrenGap: 0 }} styles={{ root: { width: 1800, backgroundColor: "#fffffff", margin: 0, paddingTop: 8 } }}>
      <Stack style={{ padding: 0 }} className={detailClasses.detailsRow}>
         <FocusZone direction={FocusZoneDirection.vertical} style={{ padding: 0 }}>
            <div className={detailClasses.container} style={{ width: "100%", height: 'calc(100vh - 278px)', position: 'relative' }} data-is-scrollable={true}>
               <ScrollablePane scrollbarVisibility={ScrollbarVisibility.always} onScroll={() => { }}>
                  <Stack tokens={{ childrenGap: 18 }} style={{ padding: 0 }}>
                     <HealthPanel desktopAlerts={false} />
                     <SchedulePanel />
                  </Stack>
               </ScrollablePane>
            </div>
         </FocusZone>
      </Stack>
   </Stack>
});

export const StreamSummary: React.FC = () => {

   const { streamId } = useParams<{ streamId: string }>();

   useEffect(() => {

      return () => {
         handler.clear();
      };

   }, []);


   if (!streamId) {
      return null;
   }

   handler.set(streamId!);

   return <StreamSummaryInner />;

};
