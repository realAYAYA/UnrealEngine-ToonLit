// Copyright Epic Games, Inc. All Rights Reserved.

import { observer } from 'mobx-react-lite';
import { ConstrainMode, DefaultButton, DetailsList, DetailsListLayoutMode, IColumn, SelectionMode, Stack, Text } from '@fluentui/react';
import React from 'react';
import { Link } from 'react-router-dom';
import { JobState, LabelState, ReportPlacement } from '../backend/Api';
import { JobDetails, JobLabel } from '../backend/JobDetails';
import { JobEventHandler } from '../backend/JobEventHandler';
import { Markdown } from '../base/components/Markdown';
import { getNiceTime } from '../base/utilities/timeUtils';
import { getLabelColor } from '../styles/colors';
import { hordeClasses } from '../styles/Styles';
import { AutosubmitInfo } from './AutoSubmit';
import { JobEventPanel } from './ErrorPane';
import { StepsPanel } from './JobDetailCommon';
import { HealthPanel } from './JobDetailHealth';
import { JobOperations } from './JobOperationsBar';
import { ChangeSummary } from './ChangeSummary';

const SummaryPanel: React.FC<{ jobDetails: JobDetails }> = observer(({ jobDetails }) => {

   if (jobDetails.updated) { }

   const text: string[] = [];

   const data = jobDetails.jobdata;
   if (!data) {
      return null;
   }

   const price = jobDetails.jobPrice();

   const timeStr = getNiceTime(jobDetails.jobdata!.createTime);

   let jobText = `Job created ${timeStr} by ${data.startedByUserInfo ? data.startedByUserInfo.name : "scheduler"} and `;

   if (jobDetails.aborted) {
      jobText += "was aborted";
      if (jobDetails.jobdata?.abortedByUserInfo) {
         jobText += ` by ${jobDetails.jobdata?.abortedByUserInfo.name}.`;
      }
   } else {
      jobText += `${data.state === JobState.Complete ? `completed ${getNiceTime(data.updateTime, false)}.` : "is currently running."}`;
   }

   text.push(jobText);

   const summary = text.join(".  ");

   const reportData = jobDetails.getReportData(ReportPlacement.Summary);

   return (<Stack styles={{ root: { paddingTop: 18, paddingRight: 12 } }}>
      <Stack className={hordeClasses.raised} >
         <Stack tokens={{ childrenGap: 12 }} grow>
            <Stack horizontal>
               <Stack>
                  <Text variant="mediumPlus" styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>Summary</Text>
               </Stack>
            </Stack>
            <Stack >
               <Text styles={{ root: { whiteSpace: "pre" } }}>{"" + summary}</Text>
               {!!reportData && <Stack style={{ paddingLeft: 0, paddingTop: 8 }}> <Markdown>{reportData}</Markdown>
               </Stack>}
               {!!price && <Stack style={{ paddingTop: 10 }}>
                  <Text>{`Estimated cost: $${price.toFixed(2)}`}</Text>
               </Stack>}
               <Stack >
                  <AutosubmitInfo jobDetails={jobDetails} />
                  <Stack>
                     <ChangeSummary streamId={jobDetails.jobdata!.streamId} change={jobDetails.jobdata!.preflightChange ?? jobDetails.jobdata!.change!} />
                  </Stack>
               </Stack>
            </Stack>
         </Stack>
      </Stack>
   </Stack>);
});


const LabelsPanel: React.FC<{ jobDetails: JobDetails }> = observer(({ jobDetails }) => {

   if (jobDetails.updated) { }

   const jobdata = jobDetails.jobdata;

   if (!jobdata || !jobDetails.stream) {
      return <div />;
   }


   const labels = jobDetails.labels.filter(label => label.stateResponse.state !== LabelState.Unspecified);

   if (!labels) {
      return <div />;
   }

   type LabelItem = {
      category: string;
      labels: JobLabel[];
   };

   const categories: Set<string> = new Set();
   labels.forEach(label => { if (label.name) { categories.add(label.category!); } });


   const items = Array.from(categories.values()).map(c => {
      return {
         category: c,
         labels: labels.filter(label => label.name && (label.category === c)).sort((a, b) => {
            return a.name! < b.name! ? -1 : 1;
         })
      } as LabelItem;
   }).filter(item => item.labels?.length).sort((a, b) => {
      return a.category < b.category ? -1 : 1;
   });

   if (!items.length) {
      return <div />;
   }

   const buildColumns = (): IColumn[] => {

      const widths: Record<string, number> = {
         "Name": 120,
         "Labels": 1108
      };

      const cnames = ["Name", "Labels"];

      return cnames.map(c => {
         return { key: c, name: c === "Status" ? "" : c, fieldName: c.replace(" ", "").toLowerCase(), minWidth: widths[c] ?? 100, maxWidth: widths[c] ?? 100, isResizable: false, isMultiline: true } as IColumn;
      });

   };

   function onRenderLabelColumn(item: LabelItem, index?: number, column?: IColumn) {

      switch (column!.key) {

         case 'Name':
            return <Stack verticalAlign="center" verticalFill={true}> <Text>{item.category}</Text> </Stack>;

         case 'Labels':
            return <Stack className="horde-no-darktheme" wrap horizontal tokens={{ childrenGap: 4 }} styles={{ root: { paddingTop: 2 } }}>
               {item.labels.map(label => {
                  const color = getLabelColor(label.stateResponse.state, label.stateResponse.outcome);
                  const labelIdx = jobDetails.labels.indexOf(label);


                  return <Link to={`/job/${jobdata!.id}?label=${labelIdx}`}>
                     <Stack className={hordeClasses.badge}>
                        <DefaultButton
                           key={label.name} style={{ backgroundColor: color.primaryColor }}
                           text={label.name!}>
                           {!!color.secondaryColor && <div style={{
                              borderLeft: "10px solid transparent",
                              borderRight: `10px solid ${color.secondaryColor}`,
                              borderBottom: "10px solid transparent",
                              height: 0,
                              width: 0,
                              position: "absolute",
                              right: 0,
                              top: 0,
                              zIndex: 1
                           }} />}

                        </DefaultButton>
                     </Stack>
                  </Link>

               })}</Stack>;

         default:
            return <span>???</span>;
      }
   }

   return (<Stack styles={{ root: { paddingTop: 18, paddingRight: 12 } }}>
      <Stack className={hordeClasses.raised}>
         <Stack tokens={{ childrenGap: 12 }}>
            <Text variant="mediumPlus" styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>Labels</Text>
            <Stack >
               <div style={{ width: 1268, borderBottom: "1px solid rgb(243, 242, 241)" }} />
               <DetailsList
                  isHeaderVisible={false}
                  indentWidth={0}
                  styles={{ root: { maxWidth: 1440 } }}
                  compact={false}
                  selectionMode={SelectionMode.none}
                  items={items}
                  columns={buildColumns()}
                  layoutMode={DetailsListLayoutMode.fixedColumns}
                  constrainMode={ConstrainMode.unconstrained}
                  onRenderItemColumn={onRenderLabelColumn}
                  onShouldVirtualize={() => { return true; }}
               />
            </Stack>
         </Stack>
      </Stack>
   </Stack>);
});


export const JobDetailOverview: React.FC<{ jobDetails: JobDetails, eventHandler: JobEventHandler }> = ({ jobDetails, eventHandler }) => {

   return <Stack styles={{ root: { width: 1340 } }}>
      <Stack styles={{ root: { paddingRight: 12 } }}>
         
      </Stack>
      <SummaryPanel jobDetails={jobDetails} />
      <LabelsPanel jobDetails={jobDetails} />
      <HealthPanel jobDetails={jobDetails} />
      {!!jobDetails?.jobdata?.preflightChange && <JobEventPanel jobDetails={jobDetails} eventHandler={eventHandler} />}
      <StepsPanel jobDetails={jobDetails} />
   </Stack>;
};