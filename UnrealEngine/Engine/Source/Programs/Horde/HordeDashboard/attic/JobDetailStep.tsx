// Copyright Epic Games, Inc. All Rights Reserved.

import { observer } from 'mobx-react-lite';
import moment from 'moment-timezone';
import { DetailsList, DetailsListLayoutMode, DetailsRow, IColumn, Icon, IDetailsListProps, SelectionMode, Stack, Text } from '@fluentui/react';
import React, { useState } from 'react';
import { Link, useHistory, useLocation } from 'react-router-dom';
import { EventSeverity, GetJobStepRefResponse, ReportPlacement } from '../backend/Api';
import dashboard from '../backend/Dashboard';
import { getStepSummaryMarkdown, JobDetails } from '../backend/JobDetails';
import { Markdown } from '../base/components/Markdown';
import { displayTimeZone, getElapsedString } from '../base/utilities/timeUtils';
import { TestReportPanel } from '../components/TestReportPanel';
import { hordeClasses, modeColors } from '../styles/Styles';
import { AbortJobModal } from './AbortJobModal';
import { AutosubmitInfo } from './AutoSubmit';
import { ChangeButton } from './ChangeButton';
import { ErrorPane } from './ErrorPane';
import { HistoryModal } from './HistoryModal';
import { JobDetailArtifacts } from './JobDetailArtifacts';
import { StepsPanel, useQuery } from './JobDetailCommon';
import { HealthPanel } from './JobDetailHealth';
import { JobOperations } from './JobOperationsBar';
import { StepRefStatusIcon } from './StatusIcon';
import { StepRetryModal, StepRetryType } from './StepRetryModal';
import { ChangeSummary } from './ChangeSummary';




const SummaryPanel: React.FC<{ jobDetails: JobDetails; stepId: string }> = observer(({ jobDetails, stepId }) => {

   if (jobDetails.updated) { }

   const jobPrice = jobDetails.jobPrice();
   const stepPrice = jobDetails.stepPrice(stepId);

   let priceText = "";

   if (stepPrice) {
      priceText = `Estimated cost: $${stepPrice.toFixed(2)}`;
      if (jobPrice) {
         priceText = `Estimated cost: $${stepPrice.toFixed(2)} (of $${jobPrice.toFixed(2)})`;
      }
   }

   const reportData = jobDetails.getReportData(ReportPlacement.Summary, stepId);

   return (<Stack styles={{ root: { paddingTop: 18, paddingRight: 12 } }}>
      <Stack className={hordeClasses.raised}>
         <Stack tokens={{ childrenGap: 18 }}>
            <Stack horizontal>
               <Stack>
                  <Text variant="mediumPlus" styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>Summary</Text>
               </Stack>
            </Stack>
            <Stack>
               <Stack style={{color: modeColors.text}}>
                  <Markdown>{getStepSummaryMarkdown(jobDetails, stepId)}</Markdown>
               </Stack>
               {!!reportData && <Stack style={{ paddingTop: 8 }}> <Markdown>{reportData}</Markdown> </Stack>}
               {!!priceText && <Stack style={{ paddingTop: 8 }}>
                  <Text>{priceText}</Text>
               </Stack>}

               <Stack tokens={{ padding: 8 }} style={{ paddingTop: 24 }}>
                  <AutosubmitInfo jobDetails={jobDetails} />
               </Stack>
               <Stack>
                  <ChangeSummary streamId={jobDetails.jobdata!.streamId} change={jobDetails.jobdata!.preflightChange ?? jobDetails.jobdata!.change!} />
               </Stack>

            </Stack>
            <StepsPanel jobDetails={jobDetails} stepId={stepId} singleStep={true} />
         </Stack>
      </Stack>
   </Stack>);
});

const HistoryPanel: React.FC<{ jobDetails: JobDetails; stepId: string }> = observer(({ jobDetails, stepId }) => {

   const history = useHistory();
   const location = useLocation();

   const stepHistory = jobDetails.history;

   // subscribe
   if (jobDetails.updated) { }

   if (stepHistory === undefined) {
      return null;
   }

   type HistoryItem = {
      ref: GetJobStepRefResponse;
   };

   const items: HistoryItem[] = stepHistory.map(h => {
      return { ref: h }
   });

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

         return <ChangeButton job={jobDetails.jobdata!} stepRef={ref} hideAborted={true} />;

      }

      if (column.name === "Agent") {

         const agentId = item.ref.agentId;

         if (!agentId) {
            return null;
         }

         const url = `${location.pathname}${location.search}&agentId=${agentId}`;

         return <a href={url} onClick={(ev) => { ev.preventDefault(); ev.stopPropagation(); history.replace(url); }}><Stack horizontal horizontalAlign={"end"} verticalFill={true} tokens={{ childrenGap: 0, padding: 0 }}><Text>{agentId}</Text></Stack></a>;
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

         if (ref.stepId === stepId && ref.jobId === jobDetails.id) {
            props.styles = { ...props.styles, root: { background: 'rgb(233, 232, 231)', selectors: { ...commonSelectors as any, "a, a:hover, a:visited": { color: "#FFFFFF" }, ":hover": { background: 'rgb(223, 222, 221)' } } } };
         } else {
            props.styles = { ...props.styles, root: { selectors: { ...commonSelectors as any } } };
         }

         return <Link to={url}><div className="job-item"><DetailsRow {...props} /> </div></Link>;

      }
      return null;
   };

   return (<Stack styles={{ root: { paddingTop: 18, paddingRight: 12 } }}>
      <Stack className={hordeClasses.raised}>
         <Stack tokens={{ childrenGap: 12 }}>
            <Text variant="mediumPlus" styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>{"History"}</Text>
            <Stack styles={{ root: { paddingLeft: 4, paddingRight: 0, paddingTop: 8, paddingBottom: 4 } }}>
               <div style={{ overflowY: 'auto', overflowX: 'hidden', maxHeight: "400px" }} data-is-scrollable={true}>
                  {!!stepHistory.length && <DetailsList
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
                  {!stepHistory.length && <Text>No step history</Text>}
               </div>
            </Stack>
         </Stack>
      </Stack>
   </Stack>);
});


const ErrorPanel: React.FC<{ jobDetails: JobDetails; stepId: string; showErrors: boolean }> = ({ jobDetails, stepId, showErrors }) => {

   return (<Stack styles={{ root: { paddingTop: 18, paddingRight: 12 } }}>
      <Stack className={hordeClasses.raised}>
         <Stack tokens={{ childrenGap: 12 }}>
            <Text variant="mediumPlus" styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>{showErrors ? "Errors" : "Warnings"}</Text>
            <Stack styles={{ root: { paddingLeft: 4, paddingRight: 0, paddingTop: 8, paddingBottom: 4 } }}>
               <ErrorPane stepId={stepId} jobDetails={jobDetails} showErrors={showErrors} />
            </Stack>
         </Stack>
      </Stack>
   </Stack>);
};

export const JobDetailStep: React.FC<{ jobDetails: JobDetails; stepId: string }> = observer(({ jobDetails, stepId }) => {

   const [shown, setShown] = useState<{ abortShown?: boolean, retryShown?: boolean }>({});
   const [runType, setRunType] = useState(StepRetryType.RunAgain);
   let [historyAgentId, setHistoryAgentId] = useState<string | undefined>(undefined);
   const history = useHistory();
   const query = useQuery();

   if (!historyAgentId && query.get("agentId")) {
      historyAgentId = query.get("agentId")!;
   }


   // reference for updates
   if (jobDetails.updated) { }

   const events = jobDetails.eventsByStep(stepId);

   const errors = events.filter(e => e.severity === EventSeverity.Error);
   const warnings = events.filter(e => e.severity === EventSeverity.Warning);

   const jobData = jobDetails.jobdata!;
   const step = jobDetails.stepById(stepId)!;

   const testdata = jobDetails.getStepTestData(stepId);

   const viewLogDisabled = !step.logId;

   const node = jobDetails.nodeByStepId(stepId);

   const canRunDisabled = !node?.allowRetry || !!step.retriedByUserInfo;
   const canTryFix = jobDetails.template?.allowPreflights;


   const url = `/log/${step!.logId}`;

   const showBuildLocally = node!.name !== "Setup Build";

   const textSize = "small";

   const viewLog = <Stack horizontal verticalAlign="center" tokens={{ childrenGap: 8 }}>
      <Icon styles={{ root: { margin: '0px', padding: '0px', paddingTop: "2px" } }} iconName="AlignLeft" className={!viewLogDisabled ? hordeClasses.iconBlue : hordeClasses.iconDisabled} />
      <Text variant={textSize} styles={{ root: { margin: '0px', padding: '0px' } }} className={viewLogDisabled ? "view-log-link-disabled" : "view-log-link"}>View Log</Text>
   </Stack>;

   const runAgain = <Stack horizontal verticalAlign="center" tokens={{ childrenGap: 8 }}>
      <Icon styles={{ root: { margin: '0px', padding: '0px', paddingTop: "0px" } }} iconName="Redo" className={!canRunDisabled ? hordeClasses.iconBlue : hordeClasses.iconDisabled} />
      <Text variant={textSize} styles={{ root: { margin: '0px', padding: '0px' } }} className={canRunDisabled ? "view-log-link-disabled" : "view-log-link"}>Run Step Again</Text>
   </Stack>;

   const abortStep = <Stack horizontal verticalAlign="center" tokens={{ childrenGap: 8 }}>
      <Icon styles={{ root: { margin: '0px', padding: '0px', paddingTop: "0px" } }} iconName="Cross" className={!step.finishTime ? hordeClasses.iconBlue : hordeClasses.iconDisabled} />
      <Text variant={textSize} styles={{ root: { margin: '0px', padding: '0px' } }} className={step.finishTime ? "view-log-link-disabled" : "view-log-link"}>Abort Step</Text>
   </Stack>;


   const testFix = <Stack horizontal verticalAlign="center" tokens={{ childrenGap: 8 }}>
      <Icon styles={{ root: { margin: '0px', padding: '0px', paddingTop: "0px" } }} iconName="Locate" className={canTryFix ? hordeClasses.iconBlue : hordeClasses.iconDisabled} />
      <Text variant={textSize} styles={{ root: { margin: '0px', padding: '0px' } }} className={!canTryFix ? "view-log-link-disabled" : "view-log-link"}>Preflight Step</Text>
   </Stack>;

   const build = <Stack horizontal verticalAlign="center" tokens={{ childrenGap: 8 }}>
      <Icon styles={{ root: { margin: '0px', padding: '0px', paddingTop: "0px" } }} iconName="Build" className={hordeClasses.iconBlue} />
      <Text variant={textSize} styles={{ root: { margin: '0px', padding: '0px' } }} className={"view-log-link"}>Build Locally</Text>
   </Stack>;

   //const notifications = <NotificationDropdown complete={stepComplete} jobDetails={jobDetails}/>;

   function buildUrl(): string {

      // @todo: we should be able to get stream from details, even if deleted from Horde
      if (!jobDetails.stream?.project) {
         return "";
      }

      const stream = `//${jobDetails.stream!.project!.name}/${jobDetails.stream!.name}`;
      const changelist = jobData.change;

      let args = "RunUAT.bat BuildGraph ";

      args += `-Target="${node!.name}" `;

      args += jobData.arguments!.map(a => {

         if (a.indexOf("Setup Build") !== -1) {
            return "";
         }

         if (a.toLowerCase().indexOf("target=") !== -1) {
            return "";
         }

         return a.indexOf(" ") === -1 ? a : `"${a}"`;
      }
      ).join(" ");

      return `ugs://execute?stream=${encodeURIComponent(stream)}&changelist=${encodeURIComponent(changelist ?? "")}&command=${encodeURIComponent(args)}`;
   }

   return <Stack styles={{ root: { width: 1340 } }}>
      <StepRetryModal stepId={stepId} jobDetails={jobDetails} type={runType} show={shown.retryShown ?? false} onClose={() => { setShown({}); }} />
      <AbortJobModal stepId={stepId} jobDetails={jobDetails} show={shown.abortShown ?? false} onClose={() => { setShown({}); }} />
      <HistoryModal agentId={historyAgentId} onDismiss={() => { history.replace(`/job/${jobDetails.id!}?step=${stepId}`); setHistoryAgentId(undefined); }} />
      <Stack horizontal styles={{ root: { paddingTop: 4 } }}>
         <Stack grow />
         <Stack horizontal tokens={{ childrenGap: 18 }} style={{ paddingRight: 12 }}>
            

            {!canRunDisabled && <div className={"view-log-link"} onClick={(ev) => { ev.preventDefault(); setShown({ retryShown: true }); setRunType(StepRetryType.RunAgain); }}>
               {runAgain}
            </div>
            }

            {!step.finishTime && <div className={"view-log-link"} onClick={(ev) => { ev.preventDefault(); setShown({ abortShown: true }); }}>
               {abortStep}
            </div>
            }


            {canRunDisabled && runAgain}

            {canTryFix && <div className={"view-log-link"} onClick={(ev) => { ev.preventDefault(); setShown({ retryShown: true }); setRunType(StepRetryType.TestFix); }}>
               {testFix}
            </div>
            }

            {showBuildLocally && <a className={"view-log-link"} href={buildUrl()}>{build}</a>}

            {!viewLogDisabled && <Link className={"view-log-link"} to={`${viewLogDisabled ? "" : url}`}>
               {viewLog}
            </Link>
            }
            {viewLogDisabled && viewLog}
         </Stack>
      </Stack>

      <SummaryPanel jobDetails={jobDetails} stepId={stepId} />
      <HealthPanel jobDetails={jobDetails} />
      {errors.length && <ErrorPanel jobDetails={jobDetails} stepId={stepId} showErrors={true} />}
      {warnings.length && <ErrorPanel jobDetails={jobDetails} stepId={stepId} showErrors={false} />}
      {testdata.length && <TestReportPanel testdata={testdata} />}
      <StepsPanel jobDetails={jobDetails} stepId={stepId} />
      <HistoryPanel jobDetails={jobDetails} stepId={stepId} />
      {jobDetails.artifacts.length !== 0 && (jobDetails.artifacts.find(a => a.stepId === stepId) != null) && <JobDetailArtifacts jobDetails={jobDetails} stepId={stepId} />}
      {/* <JobDetailTrace jobDetails={jobDetails} stepId={stepId} /> */}
   </Stack>;
});