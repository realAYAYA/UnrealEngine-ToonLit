// Copyright Epic Games, Inc. All Rights Reserved.

import { observer } from 'mobx-react-lite';
import { IColumn, List, Stack, Text } from '@fluentui/react';
import { getFocusStyle, mergeStyleSets } from '@fluentui/react/lib/Styling';
import React from 'react';
import { Link, useNavigate } from 'react-router-dom';
import { EventData, EventSeverity, GetLabelResponse } from '../backend/Api';
import { JobDetails } from '../backend/JobDetails';
import { JobEventHandler } from '../backend/JobEventHandler';
import { renderLine } from './LogRender';
import { StepStatusIcon } from './StatusIcon';
import { getHordeStyling } from '../styles/Styles';
import { getHordeTheme } from '../styles/theme';


let _styles: any;

const getStyles = () => {

   const theme = getHordeTheme();

   const styles = _styles ?? mergeStyleSets({
      gutter: [
         {
            borderLeftStyle: 'solid',
            borderLeftColor: "#EC4C47",
            borderLeftWidth: 6,
            padding: 0,
            margin: 0,
            paddingTop: 8,
            paddingBottom: 8,
            paddingRight: 8,
            marginTop: 0,
            marginBottom: 0
         }
      ],
      gutterWarning: [
         {
            borderLeftStyle: 'solid',
            borderLeftColor: "rgb(247, 209, 84)",
            borderLeftWidth: 6,
            padding: 0,
            margin: 0,
            paddingTop: 8,
            paddingBottom: 8,
            paddingRight: 8,
            marginTop: 0,
            marginBottom: 0
         }
      ],
      itemCell: [
         getFocusStyle(theme, { inset: -1 }),
         {
            selectors: {
               '&:hover': { background: theme.palette.neutralLight }
            }
         }
      ],

   });

   _styles = styles;

   return styles;
}


export const ErrorPane: React.FC<{ jobDetails: JobDetails; stepId: string; showErrors: boolean; count?: number }> = ({ jobDetails, stepId, showErrors, count }) => {

   const navigate = useNavigate();
   const styles = getStyles();

   if (!stepId) {
      return (<div />);
   }

   const events = jobDetails.eventsByStep(stepId);
   const step = jobDetails.stepById(stepId);

   if (!step) {
      return (<div />);
   }

   let errors = events.filter(e => e.severity === EventSeverity.Error).sort((a, b) => {
      return a.lineIndex < b.lineIndex ? -1 : 1;
   });

   if (count) {
      errors = errors.slice(0, count);
   }

   let warnings = events.filter(e => e.severity === EventSeverity.Warning).sort((a, b) => {
      return a.lineIndex < b.lineIndex ? -1 : 1;
   });

   if (count) {
      warnings = warnings.slice(0, count);
   }

   if (showErrors && !errors.length) {
      return <div />;
   }

   if (!showErrors && !warnings.length) {
      return <div />;
   }

   const onRenderCell = (item?: EventData, index?: number, isScrolling?: boolean): JSX.Element => {

      if (!item) {
         return <div>???</div>;
      }

      const url = `/log/${item.logId}?lineindex=${item.lineIndex}`;

      const lines = item.lines.filter(line => line.message?.trim().length).map(line => <Stack styles={{ root: { paddingLeft: 8, paddingRight: 8, lineBreak: "anywhere", whiteSpace: "pre-wrap", lineHeight: 18, fontSize: 10, fontFamily: "Horde Cousine Regular, monospace, monospace" } }}> <Link className="log-link" to={url}>{renderLine(navigate, line, undefined, {})}</Link></Stack>);

      return (<Stack className={styles.itemCell} styles={{ root: { padding: 8, marginRight: 8 } }}><Stack className={item.severity === EventSeverity.Warning ? styles.gutterWarning : styles.gutter} styles={{ root: { padding: 0, margin: 0 } }}>
         <Stack styles={{ root: { paddingLeft: 14 } }}>
            {lines}
         </Stack>
      </Stack>
      </Stack>
      );
   };

   return (<Stack>
      <Stack tokens={{ padding: 4 }}>
         {showErrors && errors.length && <Stack>
            <div style={{ overflowY: 'auto', overflowX: 'visible', maxHeight: "400px" }} data-is-scrollable={true}>
               <List
                  items={errors}
                  data-is-focusable={false}
                  onRenderCell={onRenderCell} />
               <div style={{ padding: 8 }} />
            </div>
         </Stack>}
         {!showErrors && warnings.length && <Stack>
            <div style={{ overflowY: 'auto', overflowX: 'visible', maxHeight: "400px" }} data-is-scrollable={true}>
               <List
                  items={warnings}
                  data-is-focusable={false}
                  onRenderCell={onRenderCell} />
               <div style={{ padding: 8 }} />
            </div>

         </Stack>}
      </Stack>
   </Stack>);
};

export const JobEventListPanel: React.FC<{ jobDetails: JobDetails, stepIds: string[], eventHandler: JobEventHandler }> = ({ jobDetails, stepIds, eventHandler }) => {

   const navigate = useNavigate();
   const styles = getStyles();

   if (!stepIds.length) {
      return null;
   }

   type EventItem = {

      stepId: string;
      event?: EventData;

   }

   if (!stepIds.length) {
      return null;
   }

   const items: EventItem[] = [];

   stepIds.forEach(stepId => {

      items.push({
         stepId: stepId,
      });

      eventHandler.stepEvents.get(stepId)?.events?.sort((a, b) => a.lineIndex - b.lineIndex).forEach(e => {

         items.push({
            stepId: stepId,
            event: e
         });
      })
   })

   const job = jobDetails.jobdata!;

   const renderEventRow = (item: EventItem, index?: number, column?: IColumn) => {

      if (!item) {
         return <div>???</div>;
      }

      const step = jobDetails.stepById(item.stepId)!;
      const errors = eventHandler.stepEvents.get(item.stepId)?.events?.filter(e => e.severity === EventSeverity.Error) ?? [];
      const warnings = eventHandler.stepEvents.get(item.stepId)?.events?.filter(e => e.severity === EventSeverity.Warning) ?? [];

      if (!errors.length && !warnings.length) {
         return null;
      }

      const logId = step.logId!;
      const event = item.event!;

      const url = `/log/${logId}?lineindex=${event.lineIndex}`;

      const lines = event.lines.map(line => <Stack styles={{ root: { paddingLeft: 8, paddingRight: 8, lineBreak: "anywhere", whiteSpace: "pre-wrap", lineHeight: 18, fontSize: 10, fontFamily: "Horde Cousine Regular, monospace, monospace" } }}> {renderLine(navigate, line, undefined, {})}</Stack>);
      return (
         <Link className="log-link" to={url}>
            <Stack className={styles.itemCell} styles={{ root: { padding: 8, paddingLeft: 24, marginRight: 8 } }}><Stack className={event.severity === EventSeverity.Warning ? styles.gutterWarning : styles.gutter} styles={{ root: { padding: 0, margin: 0 } }}>
               <Stack styles={{ root: { paddingLeft: 8 } }}>
                  {lines}
               </Stack>
            </Stack>
            </Stack>
         </Link>);
   };

   const onRenderCell = (item?: EventItem, index?: number, isScrolling?: boolean): JSX.Element | null => {

      if (!item) {
         return null;
      }

      if (item.event) {
         return renderEventRow(item);
      }

      const url = `/job/${job.id}?step=${item.stepId}`;

      const stepId = item.stepId;

      const step = jobDetails.stepById(stepId)!;

      return <Stack styles={{ root: { background: 'rgb(233, 232, 231)', marginRight: 8, height: 42, fontSize: 12, selectors: { "a, a:hover, a:visited": { color: "#FFFFFF" }, ":hover": { background: 'rgb(223, 222, 221)' } } } }} verticalFill={true} verticalAlign="center" >
         <Link to={url}><Stack style={{ paddingLeft: 12 }} horizontal><StepStatusIcon step={step} /><Text>{jobDetails.nodeByStepId(stepId)?.name}</Text>
         </Stack>
         </Link>
      </Stack>
   }

   return (<Stack style={{ maxHeight: 460 }}>
      <div style={{ overflowY: 'auto', overflowX: 'auto' }} data-is-scrollable={true}>
         <List
            items={items}
            data-is-focusable={false}
            onRenderCell={onRenderCell} />
      </div>
   </Stack>);
};


export const JobEventPanel: React.FC<{ jobDetails: JobDetails, label?: GetLabelResponse, eventHandler: JobEventHandler }> = observer(({ jobDetails, label, eventHandler }) => {

   const { hordeClasses } = getHordeStyling();

   eventHandler.set(jobDetails);

   // subscribe, this is a little ugly, should look into mobx pattern for this
   let updated = jobDetails.updated;
   updated = eventHandler.updated;
   // silence warning
   if (updated) { }

   let stepIds = jobDetails.getSteps().map(s => s.id);

   if (label) {
      const nodes = jobDetails.nodes?.filter(n => label.includedNodes?.find(on => on === n.name));
      stepIds = stepIds.filter(stepId => nodes.indexOf(jobDetails.nodeByStepId(stepId)!) !== -1);
   }

   stepIds = stepIds.filter(s => {
      return !!eventHandler.stepEvents.get(s)?.events?.length;
   })

   if (!stepIds.length) {
      return null;
   }

   let hasErrors = false;
   let hasWarnings = false;

   eventHandler.stepEvents.forEach(e => {

      if (stepIds.indexOf(e.stepId) === -1) {
         return;
      }

      if (e.events?.find(event => event.severity === EventSeverity.Error)) hasErrors = true;
      if (e.events?.find(event => event.severity === EventSeverity.Warning)) hasWarnings = true;

   });

   let title = hasErrors ? "Errors" : "";
   if (hasWarnings) {
      if (title) {
         title += " & Warnings";
      } else {
         title = "Warnings";
      }
   }

   return (<Stack styles={{ root: { paddingTop: 18, paddingRight: 12 } }}>
      <Stack className={hordeClasses.raised}>
         <Stack tokens={{ childrenGap: 12 }}>
            <Text variant="mediumPlus" styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>{title}</Text>
            <Stack styles={{ root: { paddingLeft: 4, paddingRight: 0, paddingTop: 8, paddingBottom: 4 } }}>
               <JobEventListPanel jobDetails={jobDetails} stepIds={stepIds} eventHandler={eventHandler} />
            </Stack>
         </Stack>
      </Stack>
   </Stack>);

});


