// Copyright Epic Games, Inc. All Rights Reserved.

import { DetailsList, DetailsListLayoutMode, DetailsRow, IColumn, IDetailsListProps, SelectionMode, Stack, Text } from "@fluentui/react";
import { observer } from "mobx-react-lite";
import moment from "moment";
import { useEffect } from "react";
import { Link, useNavigate, useLocation } from "react-router-dom";
import backend from "../../backend";
import { GetJobStepRefResponse } from "../../backend/Api";
import dashboard from "../../backend/Dashboard";
import { ISideRailLink } from "../../base/components/SideRail";
import { displayTimeZone, getElapsedString } from "../../base/utilities/timeUtils";
import { ChangeButton } from "../ChangeButton";
import { StepRefStatusIcon } from "../StatusIcon";
import { JobDataView, JobDetailsV2 } from "./JobDetailsViewCommon";
import { getHordeTheme } from "../../styles/theme";
import { getHordeStyling } from "../../styles/Styles";

const sideRail: ISideRailLink = { text: "History", url: "rail_step_history" };

class StepHistoryDataView extends JobDataView {

   filterUpdated() {
      // this.updateReady();
   }

   set(stepId?: string) {

      const details = this.details;

      if (!details) {
         return;
      }

      if (this.stepId === stepId || !details.jobId) {
         return;
      }

      this.stepId = stepId;

      if (!this.stepId) {
         return;
      }

      const stepName = details.getStepName(this.stepId, false);

      if (stepName) {
         this.loadHistory(stepName);
      }

   }

   loadHistory(stepName: string) {
      const jobData = this.details?.jobData;
      if (!jobData) {
         return;
      }
      backend.getJobStepHistory(jobData.streamId, stepName, 1024, jobData.templateId!).then(response => {
         this.history = response;         
      }).finally(() => {
         this.initialize(this.history?.length ? [sideRail] : undefined);
         this.updateReady();
      })
   }

   clear() {
      this.history = [];
      this.stepId = undefined;
      super.clear();
   }

   detailsUpdated() {
      if (!this.initialized) {
         const stepName = this.details?.getStepName(this.stepId, false);
         if (stepName) {
            this.loadHistory(stepName);
         }         
      }
   }

   history: GetJobStepRefResponse[] = [];

   stepId?: string;

   order = 5;

}

JobDetailsV2.registerDataView("StepHistoryDataView", (details: JobDetailsV2) => new StepHistoryDataView(details));


export const StepHistoryPanel: React.FC<{ jobDetails: JobDetailsV2; stepId: string }> = observer(({ jobDetails, stepId }) => {

   const navigate = useNavigate();
   const location = useLocation();

   const dataView = jobDetails.getDataView<StepHistoryDataView>("StepHistoryDataView");

   useEffect(() => {
      return () => {
         dataView?.clear();
      };
   }, [dataView]);

   dataView.subscribe();

   const { hordeClasses } = getHordeStyling();

   if (!jobDetails.jobData) {
      return null;
   }

   const theme = getHordeTheme();

   dataView.set(stepId);

   if (!jobDetails.viewReady(dataView.order)) {
      return null;
   }

   type HistoryItem = {
      ref: GetJobStepRefResponse;
   };

   const items: HistoryItem[] = dataView.history.map(h => {
      return { ref: h }
   });

   if (!items.length) {
      return null;
   }

   const columns = [
      { key: 'column1', name: 'Name', minWidth: 620, maxWidth: 620, isResizable: false },
      { key: 'column2', name: 'Change', minWidth: 80, maxWidth: 80, isResizable: false },
      { key: 'column3', name: 'Started', minWidth: 180, maxWidth: 180, isResizable: false },
      { key: 'column4', name: 'Agent', minWidth: 100, maxWidth: 100, isResizable: false },
      { key: 'column5', name: 'Duration', minWidth: 110, maxWidth: 110, isResizable: false },
   ];

   const renderItem = (item: HistoryItem, index?: number, column?: IColumn) => {

      if (!column) {
         return <div />;
      }

      const ref = item.ref;


      if (column.name === "Name") {
         return <Stack horizontal>{<StepRefStatusIcon stepRef={ref} />}<Text>{jobDetails.nodeByStepId(stepId)?.name}</Text></Stack>;
      }

      if (column.name === "Change") {

         const sindex = dataView.history.indexOf(ref);
         return <ChangeButton job={jobDetails.jobData!} stepRef={ref} hideAborted={true} rangeCL={sindex < (dataView.history.length - 1) ? (dataView.history[sindex + 1].change + 1) : undefined} />;

      }

      if (column.name === "Agent") {

         const agentId = item.ref.agentId;

         if (!agentId) {
            return null;
         }

         let url = `${location.pathname}?agentId=${agentId}`;
         if (location.search) {
            url = `${location.pathname}${location.search}&agentId=${agentId}`;
         }

         return <a href={url} onClick={(ev) => { ev.preventDefault(); ev.stopPropagation(); navigate(url, { replace: true }); }}><Stack horizontal horizontalAlign={"end"} verticalFill={true} tokens={{ childrenGap: 0, padding: 0 }}><Text>{agentId}</Text></Stack></a>;
      }

      if (column.name === "Started") {

         if (ref.startTime) {

            const displayTime = moment(ref.startTime).tz(displayTimeZone());
            const format = dashboard.display24HourClock ? "HH:mm:ss z" : "LT z";

            let displayTimeStr = displayTime.format('MMM Do') + ` at ${displayTime.format(format)}`;


            return <Stack horizontal horizontalAlign={"start"}>{displayTimeStr}</Stack>;

         } else {
            return "???";
         }
      }

      if (column.name === "Duration") {

         const start = moment(ref.startTime);
         let end = moment(Date.now());

         if (ref.finishTime) {
            end = moment(ref.finishTime);
         }
         if (item.ref.startTime) {
            const time = getElapsedString(start, end);
            return <Stack horizontal horizontalAlign={"end"} style={{ paddingRight: 8 }}><Text>{time}</Text></Stack>;
         } else {
            return "???";
         }
      }


      return <Stack />;
   }

   const renderRow: IDetailsListProps['onRenderRow'] = (props) => {

      if (props) {

         const item = props!.item as HistoryItem;
         const ref = item.ref;
         const url = `/job/${ref.jobId}?step=${ref.stepId}`;

         const commonSelectors = { ".ms-DetailsRow-cell": { "overflow": "visible" } };

         if (ref.stepId === stepId && ref.jobId === jobDetails.jobId) {
            props.styles = { ...props.styles, root: { background: `${theme.palette.neutralLight} !important`, selectors: { ...commonSelectors as any } } };
         } else {
            props.styles = { ...props.styles, root: { selectors: { ...commonSelectors as any } } };
         }

         return <Link to={url}><div className="job-item"><DetailsRow {...props} /> </div></Link>;

      }
      return null;
   };

   return (<Stack id={sideRail.url} styles={{ root: { paddingTop: 18, paddingRight: 12 } }}>
      <Stack className={hordeClasses.raised}>
         <Stack tokens={{ childrenGap: 12 }}>
            <Text variant="mediumPlus" styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>{"History"}</Text>
            <Stack styles={{ root: { paddingLeft: 4, paddingRight: 0, paddingTop: 8, paddingBottom: 4 } }}>
               <div style={{ overflowY: 'auto', overflowX: 'hidden', maxHeight: "400px" }} data-is-scrollable={true}>
                  {!!dataView.history.length && <DetailsList
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
                  />}
                  {!dataView.history.length && <Text>No step history</Text>}
               </div>
            </Stack>
         </Stack>
      </Stack>
   </Stack>);
});