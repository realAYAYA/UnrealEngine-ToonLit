// Copyright Epic Games, Inc. All Rights Reserved.

import { DetailsList, DetailsListLayoutMode, DetailsRow, IColumn, IDetailsListProps, mergeStyleSets, SelectionMode, Stack, Text } from '@fluentui/react';
import { observer } from 'mobx-react-lite';
import moment from 'moment-timezone';
import React, { useState } from 'react';
import { Link, useLocation } from 'react-router-dom';
import { GetIssueResponse, IssueData } from '../backend/Api';
import dashboard from '../backend/Dashboard';
import { getIssueSummary } from '../backend/Issues';
import { JobDetails } from '../backend/JobDetails';
import { getElapsedString, getShortNiceTime } from '../base/utilities/timeUtils';
import { hordeClasses } from '../styles/Styles';
import { IssueModalV2 } from './IssueViewV2';
import { useQuery } from './JobDetailCommon';
import { IssueStatusIcon } from './StatusIcon';

export const HealthPanel: React.FC<{ jobDetails: JobDetails }> = observer(({ jobDetails }) => {

   const query = useQuery();
   const location = useLocation();
   const [issueHistory, setIssueHistory] = useState(false);

   // subscribe
   if (jobDetails.updated) { }

   let issues: GetIssueResponse[] = Object.assign([], jobDetails.issues);

   if (!issues.length) {
      return <div />;
   }

   type HealthItem = {
      issue: GetIssueResponse;
   };

   let items: HealthItem[] = issues.map(i => {
      return { issue: i }
   });

   items = items.sort((a, b) => {
      if (a.issue.resolvedAt && !b.issue.resolvedAt) {
         return 1;
      }

      if (!a.issue.resolvedAt && b.issue.resolvedAt) {
         return -1;
      }

      return b.issue.id - a.issue.id
   });

   const columns = [
      { key: 'health_column1', name: 'Summary', minWidth: 580, maxWidth: 580, isResizable: false },
      { key: 'health_column2', name: 'Jira', minWidth: 100, maxWidth: 100, isResizable: false },
      { key: 'health_column3', name: 'Status', minWidth: 200, maxWidth: 200, isResizable: false },
      { key: 'health_column4', name: 'Opened', minWidth: 170, maxWidth: 170, isResizable: false },
   ];

   const renderItem = (item: HealthItem, index?: number, column?: IColumn) => {

      if (!column) {
         return <div />;
      }

      const issue = item.issue;

      let summary = issue.summary;

      let events = (issue as IssueData).events;

      if (summary && events?.length) {
         summary = getIssueSummary(issue, events);
      }


      let ack = issue.acknowledgedAt ? "Acknowledged" : "Unacknowledged";
      let status = "Unassigned";
      if (issue.ownerInfo) {
         status = issue.ownerInfo.name;
      }
      if (issue.resolvedAt) {
         status = "Resolved";
         if (issue.fixChange) {
            status += ` in CL ${issue.fixChange}`;
         }
      }

      if (status === "Unassigned" || status === "Resolved") {
         ack = "";
      }

      if (column.name === "Summary") {
         return <Stack horizontal disableShrink={true}>{<IssueStatusIcon issue={issue} />}<Text style={{ textDecoration: !!issue.resolvedAt ? "line-through" : undefined }}>{`Issue ${issue.id} - ${summary}`}</Text></Stack>;
      }

      if (column.name === "Status") {
         return <Stack horizontalAlign="end" disableShrink={true}><Text>{status}</Text></Stack>;
      }

      if (column.name === "Ack") {
         return <Text>{ack}</Text>;
      }

      if (column.name === "Jira") {

         if (!issue.externalIssueKey) {
            return null;
         }

         const url = `${dashboard.externalIssueService?.url}/browse/${issue.externalIssueKey}`;

         return <Stack horizontalAlign="start" disableShrink={true}><a href={url} target="_blank" rel="noreferrer" onClick={(ev) => ev.stopPropagation()}><Text>{issue.externalIssueKey}</Text> </a></Stack>;
      }




      if (column.name === "Opened") {
         const openSince = `${getShortNiceTime(issue.createdAt)} (${getElapsedString(moment(issue.createdAt), moment.utc(), false).trim()})`;
         return <Stack horizontalAlign="end"><Text>{openSince}</Text></Stack>;
      }

      return <Stack />;
   }

   const classes = mergeStyleSets({
      detailsRow: {
         selectors: {
            '.ms-DetailsRow-cell': {
               paddingLeft: 0,
               paddingRight: 0,
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

   return (<Stack styles={{ root: { paddingTop: 18, paddingRight: 12 } }}>
      <Stack className={hordeClasses.raised}>
         <Stack tokens={{ childrenGap: 12 }}>
            <Text variant="mediumPlus" styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>Health</Text>
            <Stack styles={{ root: { paddingLeft: 0, paddingRight: 0, paddingTop: 8, paddingBottom: 4 } }}>
               <IssueModalV2 issueId={query.get("issue")} popHistoryOnClose={issueHistory} />
               <DetailsList
                  compact={true}
                  isHeaderVisible={false}
                  indentWidth={0}
                  items={items}
                  columns={columns}
                  setKey="set"
                  selectionMode={SelectionMode.none}
                  layoutMode={DetailsListLayoutMode.justified}
                  onRenderItemColumn={renderItem}
                  onRenderRow={renderRow}
               />
            </Stack>
         </Stack>
      </Stack>
   </Stack>);
});