// Copyright Epic Games, Inc. All Rights Reserved.

import { List, Stack, Text } from "@fluentui/react";
import { getFocusStyle, getTheme, mergeStyleSets } from '@fluentui/react/lib/Styling';
import { observer } from "mobx-react-lite";
import React, { useEffect } from 'react';
import { Link } from 'react-router-dom';
import backend from "../../backend";
import { EventData, EventSeverity } from '../../backend/Api';
import { ISideRailLink } from "../../base/components/SideRail";
import { hordeClasses } from "../../styles/Styles";
import { renderLine } from '../LogRender';
import { JobDataView, JobDetailsV2 } from "./JobDetailsViewCommon";

const errorSideRail: ISideRailLink = { text: "Errors", url: "rail_step_errors" };
const warningSideRail: ISideRailLink = { text: "Warnings", url: "rail_step_warnings" };

class StepSummaryErrorsView extends JobDataView {

   filterUpdated() {
      // this.updateReady();
   }

   get errors() {

      return this.events.filter(e => e.severity === EventSeverity.Error).sort((a, b) => {
         return a.lineIndex < b.lineIndex ? -1 : 1;
      });

   }

   get warnings() {

      return this.events.filter(e => e.severity === EventSeverity.Warning).sort((a, b) => {
         return a.lineIndex < b.lineIndex ? -1 : 1;
      });

   }

   set(stepId: string) {

      const details = this.details;
      if (!details) {
         return;
      }

      if (this.stepId === stepId) {
         return;
      }

      this.stepId = stepId;

      if (!this.stepId) {
         this.initialize();
         this.events = [];
         return;
      }

      const step = details.stepById(stepId);

      if (!step || !step.logId) {
         this.initialize();
         this.events = [];
         return;
      }


      backend.getLogEvents(step.logId).then(events => {
         this.events = events;
         this.updateReady();
      }).finally(() => {

         const rails: ISideRailLink[] = [];
         if (this.errors.length) {
            rails.push(errorSideRail);
         }
         if (this.warnings.length) {
            rails.push(warningSideRail);
         }
         this.initialize(rails)
      });

   }

   clear() {
      this.stepId = "";
      this.events = [];
      super.clear();
   }

   detailsUpdated() {
      if (!this.details?.jobData) {
         return;
      }

      this.updateReady();
   }

   stepId: string = "";

   events: EventData[] = []

   order = 2;

}

JobDetailsV2.registerDataView("StepSummaryErrorsView", (details: JobDetailsV2) => new StepSummaryErrorsView(details));

const theme = getTheme();

const styles = mergeStyleSets({
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
            '&:hover': { background: "rgb(243, 242, 241)" }
         }
      }
   ],

});

const ErrorPane: React.FC<{ jobDetails: JobDetailsV2; view: StepSummaryErrorsView, stepId: string; showErrors: boolean; count?: number }> = ({ jobDetails, view, stepId, showErrors, count }) => {

   if (!stepId) {
      return (<div />);
   }

   let events = showErrors ? view.errors : view.warnings;

   const step = jobDetails.stepById(stepId);

   if (!step) {
      return null;
   }

   if (count) {
      events = events.slice(0, count);
   }

   if (!events.length) {
      return null;
   }

   const onRenderCell = (item?: EventData, index?: number, isScrolling?: boolean): JSX.Element => {

      if (!item) {
         return <div>???</div>;
      }

      const url = `/log/${item.logId}?lineindex=${item.lineIndex}`;

      const lines = item.lines.filter(line => line.message?.trim().length).map(line => <Stack key={`steperrorpane_line_${item.lineIndex}`} styles={{ root: { paddingLeft: 8, paddingRight: 8, lineBreak: "anywhere", whiteSpace: "pre-wrap", lineHeight: 18, fontSize: 10, fontFamily: "Horde Cousine Regular, monospace, monospace" } }}> <Link className="log-link" to={url}>{renderLine(line, undefined, {})}</Link></Stack>);

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
         <Stack>
            <div style={{ overflowY: 'auto', overflowX: 'visible', maxHeight: "400px" }} data-is-scrollable={true}>
               <List
                  items={events}
                  data-is-focusable={false}
                  onRenderCell={onRenderCell} />
               <div style={{ padding: 8 }} />
            </div>
         </Stack>
      </Stack>
   </Stack>);
};

export const StepErrorPanel: React.FC<{ jobDetails: JobDetailsV2; stepId: string, showErrors: boolean }> = observer(({ jobDetails, stepId, showErrors }) => {

   const dataView = jobDetails.getDataView<StepSummaryErrorsView>("StepSummaryErrorsView");

   useEffect(() => {
      return () => {
         dataView?.clear();
      };
   }, [dataView]);

   dataView.subscribe();   

   dataView.set(stepId);

   const events = showErrors ? dataView.errors : dataView.warnings;

   if (!events.length || !jobDetails?.viewsReady) {
      return null;
   }

   return (<Stack id={showErrors ? errorSideRail.url : warningSideRail.url} styles={{ root: { paddingTop: 18, paddingRight: 12 } }}>
      <Stack className={hordeClasses.raised}>
         <Stack tokens={{ childrenGap: 12 }}>
            <Text variant="mediumPlus" styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>{showErrors ? "Errors" : "Warnings"}</Text>
            <Stack styles={{ root: { paddingLeft: 4, paddingRight: 0, paddingTop: 8, paddingBottom: 4 } }}>
               <ErrorPane view={dataView} stepId={stepId} jobDetails={jobDetails} showErrors={showErrors} />
            </Stack>
         </Stack>
      </Stack>
   </Stack>);
});

