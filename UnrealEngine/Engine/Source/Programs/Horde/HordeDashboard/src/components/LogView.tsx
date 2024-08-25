// Copyright Epic Games, Inc. All Rights Reserved.

import { Callout, DefaultButton, DetailsList, DetailsListLayoutMode, DetailsRow, DirectionalHint, Dropdown, FocusZone, FocusZoneDirection, FontIcon, IColumn, Icon, IconButton, IContextualMenuItem, IContextualMenuProps, IDetailsListProps, ITextField, List, Modal, PrimaryButton, ProgressIndicator, ScrollToMode, Selection, SelectionMode, SelectionZone, Separator, Spinner, SpinnerSize, Stack, Text, TextField, TooltipHost } from '@fluentui/react';
import { action, makeObservable, observable } from 'mobx';
import { observer } from 'mobx-react-lite';
import moment from 'moment-timezone';
import React, { useEffect, useId, useState } from 'react';
import { Link, useLocation, useNavigate, useParams } from 'react-router-dom';
import backend from '../backend';
import { ArtifactContextType, ArtifactData, EventSeverity, GetChangeSummaryResponse, GetJobStepRefResponse, GetLogEventResponse, LogLevel } from '../backend/Api';
import { CommitCache } from '../backend/CommitCache';
import dashboard from '../backend/Dashboard';
import { JobDetails } from '../backend/JobDetails';
import { Markdown } from '../base/components/Markdown';
import { useWindowSize } from '../base/utilities/hooks';
import { displayTimeZone, getElapsedString } from '../base/utilities/timeUtils';
import { getHordeStyling } from '../styles/Styles';
import { getHordeTheme } from '../styles/theme';
import { JobArtifactsModal } from './artifacts/ArtifactsModal';
import { Breadcrumbs } from './Breadcrumbs';
import { ChangeContextMenu, ChangeContextMenuTarget } from './ChangeButton';
import { HistoryModal } from './HistoryModal';
import { IssueModalV2 } from './IssueViewV2';
import { JobDetailArtifacts } from './JobDetailArtifacts';
import { useQuery } from './JobDetailCommon';
import { LogItem, renderLine } from './LogRender';
import { JobLogSource, LogSource } from './LogSource';
import { getLogStyles, logMetricNormal, logMetricSmall } from "./LogStyle";
import { PrintException } from './PrintException';
import { StepRefStatusIcon } from './StatusIcon';
import { TopNav } from './TopNav';

class LogHandler {

   constructor() {
      makeObservable(this);
      LogHandler.instance = this;
   }

   static clear() {
      this.instance?.logSource?.clear();
      this.instance = undefined;
   }

   stopTrailing() {
      this.scroll = undefined;
      this.trailing = false;
   }

   logSource?: LogSource;
   currentWarning: number | undefined;
   currentError: number | undefined;
   trailing?: boolean;
   scroll?: number;
   initialRender = true;

   infoLine?: number;

   // could be a preference
   compact = false;

   get lineHeight(): number {

      if (!this.logSource?.logItems) {
         return logMetricNormal.lineHeight;
      }

      return !this.compact ? logMetricNormal.lineHeight : logMetricSmall.lineHeight;

   }

   get fontSize(): number {

      if (!this.logSource?.logItems) {
         return logMetricNormal.fontSize;
      }

      return !this.compact ? logMetricNormal.fontSize : logMetricSmall.fontSize;

   }

   get style(): any {

      const { logStyleNormal, logStyleSmall } = getLogStyles();

      if (!this.logSource?.logItems) {
         return logStyleNormal;
      }

      return !this.compact ? logStyleNormal : logStyleSmall;

   }

   get lineRenderStyle(): any {

      const { lineRenderStyleNormal, lineRenderStyleSmall } = getLogStyles();

      if (!this.logSource?.logItems) {
         return lineRenderStyleNormal;
      }

      return !this.compact ? lineRenderStyleNormal : lineRenderStyleSmall;

   }


   @action
   externalUpdate() {
      this.updated++;
   }

   @observable
   updated = 0


   private static instance?: LogHandler;

}

let listRef: List | undefined;

const selection = new Selection({ selectionMode: SelectionMode.multiple });

const searchBox = React.createRef<ITextField>();

let curSearchIdx = 0;

let logListKey = 0;

// have to be available to a globlao keyboard handler
let globalHandler: LogHandler | undefined;
let globalSearchState: { search?: string, results?: number[], curRequest?: any } | undefined;

const searchUp = () => {

   if (globalHandler) {
      globalHandler.currentWarning = undefined;
      globalHandler.currentError = undefined;
   }

   if (globalSearchState?.results?.length) {

      curSearchIdx--;
      if (curSearchIdx < 0) {
         curSearchIdx = globalSearchState.results.length - 1;
      }

      globalHandler?.stopTrailing();
      let lineIdx = globalSearchState.results[curSearchIdx] - 10;
      if (lineIdx < 0) {
         lineIdx = 0;
      }

      if (globalHandler) {
         listRef?.scrollToIndex(lineIdx, (index) => { return globalHandler!.lineHeight; }, ScrollToMode.top);
      }


      globalHandler?.externalUpdate();

   }

}

const searchDown = () => {

   if (globalHandler) {
      globalHandler.currentWarning = undefined;
      globalHandler.currentError = undefined;
   }

   if (globalSearchState?.results?.length) {

      curSearchIdx++;
      if (curSearchIdx >= globalSearchState.results.length) {
         curSearchIdx = 0;
      }

      globalHandler?.stopTrailing();
      let lineIdx = globalSearchState.results[curSearchIdx] - 10;
      if (lineIdx < 0) {
         lineIdx = 0;
      }

      if (globalHandler) {
         listRef?.scrollToIndex(lineIdx, (index) => { return globalHandler!.lineHeight; }, ScrollToMode.top);
      }

      globalHandler?.externalUpdate();

   }

}

const commitCache = new CommitCache();

const StepHistoryModal: React.FC<{ jobDetails: JobDetails, stepId: string | undefined, onClose: () => void }> = observer(({ jobDetails, stepId, onClose }) => {

   const navigate = useNavigate();
   const location = useLocation();
   const [commitState, setCommitState] = useState<{ target?: ChangeContextMenuTarget, commit?: GetChangeSummaryResponse, rangeCL?: number }>({});
   const [stepHistory, setStepHistory] = useState<GetJobStepRefResponse[] | undefined>(undefined);

   const { hordeClasses } = getHordeStyling();
   const hordeTheme = getHordeTheme();

   const jobData = jobDetails.jobdata;

   if (!jobData || !jobData.streamId || !jobData.templateId) {
      return null;
   }

   if (stepHistory === undefined) {
      backend.getJobStepHistory(jobData.streamId, jobDetails.getStepName(stepId, false), 1024, jobData!.templateId!).then(r => {
         setStepHistory(r);
      })

      return <Modal isOpen={true} styles={{ main: { padding: 8, width: 1084, height: '624px', backgroundColor: hordeTheme.horde.contentBackground } }} className={hordeClasses.modal} onDismiss={() => { onClose() }}>
         <Stack style={{ paddingTop: 24 }} horizontalAlign='center' tokens={{ childrenGap: 18 }}>
            <Stack>
               <Text variant='mediumPlus'>Loading Step History</Text>
            </Stack>
            <Spinner size={SpinnerSize.large} />
         </Stack>
      </Modal>
   }

   // subscribe
   if (commitCache.updated) { }
   if (jobDetails.updated) { }

   if (!stepId || !stepHistory?.length) {
      return null;
   }

   type HistoryItem = {
      ref: GetJobStepRefResponse;
   };

   const items: HistoryItem[] = stepHistory.map(h => {
      return { ref: h }
   });

   const columns = [
      { key: 'column1', name: 'Name', minWidth: 400, maxWidth: 400, isResizable: false },
      { key: 'column2', name: 'Change', minWidth: 100, maxWidth: 100, isResizable: false },
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
         return <Stack horizontal verticalAlign="center" tokens={{ childrenGap: 0, padding: 0 }} style={{ width: "100%", height: "100%" }} >{<StepRefStatusIcon stepRef={ref} />}<Text>{jobDetails.nodeByStepId(stepId)?.name}</Text></Stack>;
      }

      if (column.name === "Change") {

         if (item.ref.change && jobDetails.stream?.id) {

            return <Stack verticalAlign="center" horizontalAlign="center" tokens={{ childrenGap: 0, padding: 0 }} style={{ width: "100%", height: "100%" }} onClick={async (ev) => {
               ev?.stopPropagation();
               ev?.preventDefault();

               let commit = commitCache.getCommit(jobDetails.stream!.id, item.ref.change);

               if (!commit) {
                  await commitCache.set(jobDetails.stream!.id, [item.ref.change]);
               }

               commit = commitCache.getCommit(jobDetails.stream!.id, item.ref.change);

               if (commit) {
                  const index = stepHistory.indexOf(item.ref);
                  let rangeCL: number | undefined;
                  if (index < stepHistory.length - 1) {
                     rangeCL = stepHistory[index + 1].change;
                  }
                  setCommitState({ commit: commit, rangeCL: rangeCL, target: { point: { x: ev.clientX, y: ev.clientY } } })
               }
            }}>
               <Text style={{ color: "rgb(0, 120, 212)" }}>{item.ref.change}</Text>

            </Stack>
         }

         return null;
      }

      if (column.name === "Agent") {

         const agentId = item.ref.agentId;

         if (!agentId) {
            return null;
         }

         const url = `${location.pathname}?agentId=${agentId}`;

         return <Stack verticalAlign="center" horizontalAlign="center" tokens={{ childrenGap: 0, padding: 0 }} style={{ width: "100%", height: "100%" }}>
            <a href={url} onClick={(ev) => { ev.preventDefault(); ev.stopPropagation(); navigate(url, { replace: true }); }}><Stack horizontal horizontalAlign={"end"} verticalFill={true} tokens={{ childrenGap: 0, padding: 0 }}><Text>{agentId}</Text></Stack></a>
         </Stack>
      }

      if (column.name === "Started") {

         if (ref.startTime) {

            const displayTime = moment(ref.startTime).tz(displayTimeZone());
            const format = dashboard.display24HourClock ? "HH:mm:ss z" : "LT z";

            let displayTimeStr = displayTime.format('MMM Do') + ` at ${displayTime.format(format)}`;


            return <Stack verticalAlign="center" horizontalAlign="start" tokens={{ childrenGap: 0, padding: 0 }} style={{ width: "100%", height: "100%" }}>
               <Stack >{displayTimeStr}</Stack>
            </Stack>;

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
            return <Stack verticalAlign="center" horizontalAlign="end" tokens={{ childrenGap: 0, padding: 0 }} style={{ width: "100%", height: "100%" }}><Stack style={{ paddingRight: 8 }}><Text>{time}</Text></Stack></Stack>;
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

         const url = `/log/${ref.logId}`;

         const commonSelectors = { ".ms-DetailsRow-cell": { "overflow": "visible", padding: 0 } };

         if (ref.stepId === stepId && ref.jobId === jobDetails.id) {
            props.styles = { ...props.styles, root: { background: `${hordeTheme.palette.neutralLight} !important`, selectors: { ...commonSelectors as any } } };
         } else {
            props.styles = { ...props.styles, root: { selectors: { ...commonSelectors as any } } };
         }

         return <Link to={url} onClick={(ev) => { if (!ev.ctrlKey) onClose() }}><div className="job-item"><DetailsRow {...props} /> </div></Link>;

      }
      return null;
   };

   return (<Modal isOpen={true} styles={{ main: { padding: 8, width: 1084, height: '624px', backgroundColor: hordeTheme.horde.contentBackground } }} className={hordeClasses.modal} onDismiss={() => { onClose() }}>
      {commitState.target && <ChangeContextMenu target={commitState.target} job={jobDetails.jobdata} commit={commitState.commit} rangeCL={commitState.rangeCL} onDismiss={() => setCommitState({})} />}
      <Stack styles={{ root: { paddingTop: 8, paddingLeft: 24, paddingRight: 12, paddingBottom: 8 } }}>
         <Stack tokens={{ childrenGap: 12 }}>
            <Stack horizontal styles={{ root: { padding: 8 } }}>
               <Stack style={{ paddingLeft: 8, paddingTop: 4 }} grow>
                  <Text variant="mediumPlus" styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>{"Step History"}</Text>
               </Stack>
               <Stack grow horizontalAlign="end">
                  <IconButton
                     iconProps={{ iconName: 'Cancel' }}
                     onClick={() => { onClose(); }}
                  />
               </Stack>
            </Stack>

            <Stack styles={{ root: { paddingLeft: 4, paddingRight: 0, paddingTop: 8, paddingBottom: 4 } }}>
               <div style={{ overflowY: 'auto', overflowX: 'hidden', height: "504px" }} data-is-scrollable={true}>
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
               </div>
            </Stack>
         </Stack>
      </Stack>
   </Modal>);
});

const StepArtifactsModal: React.FC<{ jobDetails: JobDetails, stepId: string | undefined, onClose: () => void }> = observer(({ jobDetails, stepId, onClose }) => {

   const { hordeClasses } = getHordeStyling();

   let artifacts: ArtifactData[] = jobDetails.artifacts;
   if (stepId) {
      artifacts = artifacts.filter(artifact => artifact.stepId === stepId);
   }

   let height = Math.min(36 * artifacts.length + 60, 500) + 200;

   const hordeTheme = getHordeTheme();

   return (<Modal isOpen={true} styles={{ main: { padding: 8, width: 1084, height: height, backgroundColor: hordeTheme.horde.contentBackground } }} className={hordeClasses.modal} onDismiss={() => { onClose() }}>

      <Stack styles={{ root: { paddingTop: 8, paddingLeft: 24, paddingRight: 12, paddingBottom: 16 } }}>
         <Stack tokens={{ childrenGap: 12 }}>
            <Stack horizontal styles={{ root: { padding: 8 } }}>
               <Stack grow horizontalAlign="end">
                  <IconButton
                     iconProps={{ iconName: 'Cancel' }}
                     onClick={() => { onClose(); }}
                  />
               </Stack>
            </Stack>

            <Stack styles={{ root: { paddingLeft: 4, paddingRight: 0, paddingBottom: 4 } }}>
               <JobDetailArtifacts jobDetails={jobDetails} stepId={stepId} topPadding={0} />
            </Stack>
         </Stack>
      </Stack>
   </Modal>);

});

const LogProgressIndicator: React.FC<{ logSource: LogSource }> = observer(({ logSource }) => {

   // subscribe
   const details = (logSource as JobLogSource).jobDetails;
   if (details?.updated) { }

   const percentComplete = logSource.percentComplete;

   return <Stack>
      <ProgressIndicator percentComplete={percentComplete} barHeight={2} styles={{ root: { paddingLeft: 12, width: 300 } }} />
   </Stack>
});


export const LogList: React.FC<{ logId: string }> = observer(({ logId }) => {

   const windowSize = useWindowSize();
   const query = useQuery();
   const location = useLocation();
   const navigate = useNavigate();
   const [tsFormat, setTSFormat] = useState('relative');
   const [handler, setHandler] = useState<LogHandler | undefined>(undefined);
   const [searchState, setSearchState] = useState<{ search?: string, results?: number[], curRequest?: any }>({});
   const [issueHistory, setIssueHistory] = useState(false);
   const [logHistory, setLogHistory] = useState(false);
   const [logArtifacts, setLogArtifacts] = useState("");
   const [logError, setLogError] = useState("")

   let [historyAgentId, setHistoryAgentId] = useState<string | undefined>(undefined);

   if (!historyAgentId && query.get("agentId")) {
      historyAgentId = query.get("agentId")!;
   }

   const artifactContext = !!query.get("artifactContext") ? query.get("artifactContext")! as ArtifactContextType : undefined;
   const artifactPath = !!query.get("artifactPath") ? query.get("artifactPath")! : undefined;

   globalHandler = handler;
   globalSearchState = searchState;
   const vw = Math.max(document.documentElement.clientWidth, window.innerWidth || 0);

   let inTimeout = false;

   useEffect(() => {

      const handler = (e: KeyboardEvent) => {


         if (e.keyCode === 114) {
            e.preventDefault();
            if (e.shiftKey) {
               searchUp();
            } else {
               searchDown();
            }

         }

         if ((e.ctrlKey || e.metaKey) && e.keyCode === 70) {
            e.preventDefault();
            searchBox.current?.focus();
         }
      }

      window.addEventListener("keydown", handler);

      return () => {
         globalHandler = globalSearchState = undefined;
         window.removeEventListener("keydown", handler);
      };

   }, []);

   if (logError) {
      return <Stack horizontalAlign='center' style={{ paddingTop: 24 }} >
         <Text variant='mediumPlus'>{`Unable to load log data - ${logError}`}</Text>
      </Stack>
   }

   if (handler && handler.logSource && handler.logSource.logId !== logId) {
      selection.setItems([], true);
      LogHandler.clear();

      setHandler(undefined);
   }

   const hordeTheme = getHordeTheme();
   const { hordeClasses, modeColors } = getHordeStyling();

   if (!logId) {
      console.error("Bad log id settings up LogList");
      return <div>Error</div>;
   }

   if (handler?.logSource?.fatalError) {

      const error = `Error getting job data, please check that you are logged in and that the link is valid.\n\n${handler.logSource.fatalError}`;
      return <Stack horizontal style={{ paddingTop: 48 }}>
         <div key={`windowsize_logview1_${windowSize.width}_${windowSize.height}`} style={{ width: vw / 2 - 720, flexShrink: 0, backgroundColor: hordeTheme.horde.contentBackground }} />
         <Stack horizontalAlign="center" style={{ width: 1440 }}><PrintException message={error} /></Stack>
      </Stack>
   }

   if (!handler) {
      LogSource.create(logId, query).then(async (source) => {
         await source.init();

         const handler = new LogHandler();

         if (!source.startLine && source.active) {
            handler.trailing = true;
         }

         handler.logSource = source;
         setHandler(handler);
      }).catch((reason) => {
         setLogError(reason);
      });

      return <Spinner size={SpinnerSize.large} />;
   };

   // subscribe
   if (handler.updated) { }

   const logSource = handler.logSource!;
   const warnings = logSource.warnings.sort((a, b) => a.lineIndex - b.lineIndex);
   const errors = logSource.errors.sort((a, b) => a.lineIndex - b.lineIndex);
   const issues = logSource.issues;

   handler.compact = !!(logSource.logItems?.length > 1000000);

   const onRenderCell = (item?: LogItem, index?: number, isScrolling?: boolean): JSX.Element => {

      const LogCell: React.FC = observer(() => {

         if (handler.updated) { }

         if (!item) {
            return (<div style={{ height: handler.lineHeight }} />);
         }

         const styles = handler.style;

         if (!item.requested) {
            let index = item.lineNumber - 50;
            if (index < 0) {
               index = 0;
            }

            logSource.loadLines(index, 100);

            return <Stack key={`key_log_line_${item.lineNumber}`} style={{ width: "max-content", height: handler.lineHeight }}>
               <div style={{ position: "relative" }}>
                  <Stack className={styles.logLine} tokens={{ childrenGap: 8 }} horizontal disableShrink={true}>
                     <Stack styles={{ root: { color: "#c0c0c0", width: 48, textAlign: "right", userSelect: "none", fontSize: handler.fontSize } }}>{item.lineNumber}</Stack>
                     <Stack className={styles.logLine} horizontal disableShrink={true} >
                        <Stack className={styles.gutter}></Stack>
                        <Stack styles={{ root: { color: "#8a8a8a", width: 82, whiteSpace: "nowrap", fontSize: handler.fontSize, userSelect: "none" } }}> </Stack>
                        <div className={styles.logLineOuter}> <Stack styles={{ root: { paddingLeft: 8, paddingRight: 8 } }}> </Stack></div>
                     </Stack>
                  </Stack>
               </div>
            </Stack>



         }

         const line = item.line;
         const warning = line?.level === LogLevel.Warning;
         const error = line?.level === LogLevel.Error;

         const ev = logSource.events.find(event => event.lineIndex === item.lineNumber - 1);

         if (ev && ev.issueId) {
            item.issueId = ev.issueId;
            item.issue = issues.find(issue => issue.id === ev.issueId);
         }

         const style = warning ? styles.itemWarning : error ? styles.itemError : styles.logLine;
         const gutterStyle = warning ? styles.gutterWarning : error ? styles.gutterError : styles.gutter;

         let timestamp = "";
         let tsWidth = 82;
         if (line && line.time && logSource.startTime) {

            if (tsFormat === 'relative') {
               const start = logSource.startTime;
               const end = moment.utc(line.time);
               if (end >= start) {
                  const duration = moment.duration(end.diff(start));
                  const hours = Math.floor(duration.asHours());
                  const minutes = duration.minutes();
                  const seconds = duration.seconds();
                  timestamp = `[${hours <= 9 ? "0" + hours : hours}:${minutes <= 9 ? "0" + minutes : minutes}:${seconds <= 9 ? "0" + seconds : seconds}]`;
               } else {
                  timestamp = "[00:00:00]";
               }
            }

            if (tsFormat === 'utc' || tsFormat === 'local') {
               tsWidth = 112;
               let tm = moment.utc(line.time);
               if (tsFormat === 'local') {
                  tm = tm.local();
               }
               timestamp = `[${tm.format("MMM DD HH:mm:ss")}]`;
            }
         }

         const IssueButton: React.FC<{ item: LogItem, event: GetLogEventResponse }> = ({ item, event }) => {

            const tooltipId = useId();

            const error = event.severity === EventSeverity.Error;

            const issueId = item!.issueId!.toString();

            query.set("issue", issueId);
            query.toString();

            // this href is for when log is copy/pasted, note that we need the onClick handler when on site, so don't reload page
            const href = `${location.pathname}?${query.toString()}`;

            const fontStyle = {
               fontFamily: "Horde Open Sans SemiBold, sans-serif, sans-serif", color: error ? "#FFFFFF" : modeColors.text, fontSize: handler.fontSize, textDecoration: !item!.issue?.resolvedAt ? undefined : "line-through"
            }

            return <Stack>
               <TooltipHost
                  content={timestamp}
                  id={tooltipId}
                  calloutProps={{ gapSpace: 0 }}
                  styles={{ root: { display: 'inline-block' } }}>
                  <DefaultButton className={error ? styles.errorButton : styles.warningButton}
                     href={href}
                     style={{ padding: 0, margin: 0, width: tsWidth, paddingLeft: 8, paddingRight: 8, height: "100%", fontWeight: "unset" }}

                     onClick={(ev) => {
                        ev.preventDefault();
                        ev.stopPropagation();
                        location.search = `?issue=${issueId}`;
                        setIssueHistory(true);
                        navigate(location);
                     }}>
                     <Text variant="small" style={{ ...fontStyle }}>Issue</Text><div style={{ ...fontStyle }}>&nbsp;</div><Text variant="small" style={{ ...fontStyle }}>{`${issueId}`}</Text>
                  </DefaultButton>
               </TooltipHost>
            </Stack>
         }

         let prefix = "";
         const lineIndex = item.lineNumber - 1;
         if (errors.length && handler.currentError !== undefined) {
            const error = errors[handler.currentError];
            if (lineIndex >= error.lineIndex && lineIndex < error.lineIndex + error.lineCount) {
               prefix = ">>> ";
            }
         }
         else if (warnings.length && handler.currentWarning !== undefined) {
            const warning = warnings[handler.currentWarning];
            if (lineIndex >= warning.lineIndex && lineIndex < warning.lineIndex + warning.lineCount) {
               prefix = ">>> ";
            }
         } else if (globalSearchState?.results?.length && curSearchIdx < globalSearchState.results.length) {
            if (lineIndex === globalSearchState.results[curSearchIdx]) {
               prefix = ">>> ";
            }
         } else {

            if (query.get("lineindex")) {
               if (lineIndex === parseInt(query.get("lineindex")!)) {
                  prefix = ">>> ";
               }
            }
         }

         const eyeColor = modeColors.text + "44";

         return (
            <Stack key={`key_log_line_${item.lineNumber}`} style={{ width: "max-content", height: handler.lineHeight }} onClick={() => {
               const search = new URLSearchParams(window.location.search);
               search.set("lineindex", (item.lineNumber - 1).toString());
               const url = `${window.location.pathname}?` + search.toString();

               navigate(url, { replace: true })
            }}>
               <div style={{ position: "relative" }}>
                  <Stack className={styles.logLine} style={{ position: "relative" }} tokens={{ childrenGap: 8 }} horizontal disableShrink={true}>
                     <Stack styles={{ root: { color: "#c0c0c0", width: 80, textAlign: "right", userSelect: "none", fontSize: handler.fontSize } }}>{prefix + item.lineNumber}</Stack>
                     <Stack className={style} horizontal disableShrink={true}>
                        <Stack className={gutterStyle}></Stack>
                        {(!item.issueId || !ev) && <Stack styles={{ root: { color: "#8a8a8a", width: tsWidth, whiteSpace: "nowrap", fontSize: handler.fontSize, userSelect: "none" } }}> {timestamp}</Stack>}
                        {!!item.issueId && !!ev && <IssueButton item={item} event={ev!} />}
                        <div className={styles.logLineOuter}> <Stack styles={{ root: { paddingLeft: 8, paddingRight: 8, position: "relative", verticalAlign: "center" } }}> {renderLine(navigate, item.line, item.lineNumber, handler.lineRenderStyle, searchState.search)}
                           <Stack id={`callout_target_${item?.lineNumber}`} style={{ position: "absolute", cursor: "pointer", userSelect: "none", left: "-12px", top: "0px" }} onClick={() => {
                              handler.infoLine = item.lineNumber;
                              handler.externalUpdate();
                           }}><FontIcon id="infoview" style={{ fontSize: 14, color: eyeColor }} iconName="Eye" /></Stack>
                           {handler.infoLine === item.lineNumber && <Callout
                              styles={{ root: { padding: "32px 24px", maxWidth: 1300 } }}
                              role="dialog"
                              gapSpace={12}
                              target={`#callout_target_${item?.lineNumber}`}
                              isBeakVisible={true}
                              beakWidth={12}
                              onDismiss={() => {
                                 handler.infoLine = undefined;
                                 handler.externalUpdate();
                              }}
                              directionalHint={DirectionalHint.rightCenter}
                              setInitialFocus>
                              <Stack style={{ maxWidth: 1140 }}>
                                 <Stack style={{ paddingBottom: 24 }}>
                                    <Text style={{ fontSize: 14, fontFamily: "Horde Open Sans SemiBold" }}>Structured Log Line</Text>
                                 </Stack>
                                 <Stack style={{ paddingLeft: 12 }}>
                                    <Text style={{ fontSize: 11, whiteSpace: "pre-wrap", fontFamily: "Horde Cousine Regular" }}>{JSON.stringify(item.line, undefined, 2).replaceAll("\\r", "").replaceAll("\\n", "\n")}</Text></Stack>
                              </Stack>
                           </Callout>}

                        </Stack>
                        </div>
                     </Stack>
                  </Stack>
               </div>
            </Stack>

         );
      });

      return <LogCell />
   };

   if (handler.trailing) {
      listRef?.scrollToIndex(logSource.logData!.lineCount - 1, undefined, ScrollToMode.bottom);
   }

   const downloadProps: IContextualMenuProps = {
      items: [
         {
            key: 'downloadLog',
            text: 'Download Text',
            onClick: () => {
               logSource.download(false);
            }
         },
         {
            key: 'downloadJson',
            text: 'Download JSON',
            onClick: () => {
               logSource.download(true);
            }
         },
      ],
      directionalHint: DirectionalHint.bottomRightEdge
   };

   let summaryText = logSource.summary;

   let baseUrl = location.pathname;

   /*
   if (agentId && summaryText) {
       summaryText += ` on agent`;
       summary = <Stack horizontal verticalAlign="center" onClick={() => { history.replace(agentUrl) }} style={{ cursor: "pointer" }}>
           <Text styles={{ root: { fontFamily: "Horde Open Sans SemiBold", color: "rgb(97, 110, 133)", paddingLeft: 4 } }}>{summaryText}</Text>
           <Stack horizontal>
               <Text styles={{ root: { fontFamily: "Horde Open Sans SemiBold", paddingLeft: 4, color: "rgb(0, 120, 212)" } }}>{agentId}</Text>
           </Stack>
       </Stack>;

   } else if (summaryText) {
       summary = <Text styles={{ root: { fontFamily: "Horde Open Sans SemiBold", color: "rgb(97, 110, 133)", paddingLeft: 4 } }}>{summaryText}</Text>;
   }
   */

   const doQuery = (newValue: string) => {

      handler.currentWarning = undefined;
      handler.currentError = undefined;

      if (!newValue) {

         setSearchState({});

      } else if (newValue !== searchState.search) {

         const request = backend.searchLog(logId, newValue, 0, 65535);

         let newState = {
            curRequest: request
         }

         request.then(logResponse => {

            if (newState.curRequest === request) {

               curSearchIdx = 0;

               const lines = logResponse.lines;


               if (lines.length) {

                  logListKey++;

                  setSearchState({
                     search: newValue,
                     curRequest: undefined,
                     results: lines
                  });


                  handler.stopTrailing();
                  let lineIdx = lines[curSearchIdx] - 10;
                  if (lineIdx < 0) {
                     lineIdx = 0;
                  }

                  // oof
                  inTimeout = true;
                  setTimeout(() => {
                     inTimeout = false;
                     listRef?.scrollToIndex(lineIdx, () => { return handler.lineHeight; }, ScrollToMode.top);
                  }, 250)

               } else {
                  setSearchState({
                     search: newValue,
                     curRequest: undefined,
                     results: lines
                  });

               }

            };
         });

         setSearchState(newState);

      } else {

         curSearchIdx = 0;

         if (searchState.results?.length) {
            handler.stopTrailing();
            let lineIdx = searchState.results[curSearchIdx] - 10;
            if (lineIdx < 0) {
               lineIdx = 0;
            }
            listRef?.scrollToIndex(lineIdx, (index) => { return handler.lineHeight; }, ScrollToMode.top);
         }

      }

   }

   // we need job details here, though need to make a better accessor
   const fixme = (logSource as any).jobDetails as JobDetails | undefined;
   const menuProps: IContextualMenuProps = { items: [] };

   if (fixme) {

      let artifacts: ArtifactData[] = [];

      if (fixme.artifacts) {
         const artifactStepId = fixme!.stepByLogId(logId)?.id
         if (artifactStepId) {
            artifacts = fixme.artifacts.filter(artifact => artifact.stepId === artifactStepId);
         }
      }

      if (!fixme.jobdata?.useArtifactsV2) {
         menuProps.items.push({
            key: 'jobstep_artifacts',
            disabled: artifacts.length === 0,
            text: `Step Artifacts`,
            onClick: () => setLogArtifacts("legacy")
         })
      } else {

         const stepArtifacts = (logSource as JobLogSource).artifactsV2;

         const atypes = new Map<ArtifactContextType, number>();
         const knownTypes = new Set<string>(["step-saved", "step-output", "step-trace"]);

         stepArtifacts?.forEach(a => {
            let c = atypes.get(a.type) ?? 0;
            c++;
            atypes.set(a.type, c);
         });

         const opsList: IContextualMenuItem[] = [];

         const navigateToArtifacts = (context: string) => {
            const search = new URLSearchParams(window.location.search);
            search.set("artifactContext", encodeURIComponent(context));
            const url = `${window.location.pathname}?` + search.toString();
            navigate(url, { replace: true })
         }

         opsList.push({
            key: 'stepops_artifacts_step',
            text: "Logs",
            iconProps: { iconName: "Folder" },
            disabled: !atypes.get("step-saved"),
            onClick: () => {
               navigateToArtifacts("step-saved");
            }
         });

         opsList.push({
            key: 'stepops_artifacts_output',
            text: "Temp Storage",
            iconProps: { iconName: "MenuOpen" },
            disabled: !atypes.get("step-output"),
            onClick: () => {
               navigateToArtifacts("step-output");
            }
         });

         opsList.push({
            key: 'stepops_artifacts_trace',
            text: "Traces",
            iconProps: { iconName: "SearchTemplate" },
            disabled: !atypes.get("step-trace"),
            onClick: () => {
               navigateToArtifacts("step-trace");
            }
         });

         const custom = stepArtifacts?.filter(a => !knownTypes.has(a.type)).sort((a, b) => a.type.localeCompare(b.type));
         custom?.forEach(c => {
            opsList.push({
               key: `stepops_artifacts_${c.type}`,
               text: c.description ?? c.name,
               iconProps: { iconName: "Clean" },
               onClick: () => { navigateToArtifacts(c.type) }
            });
         })


         menuProps.items.push({
            key: 'jobstep_artifacts',
            text: `Artifacts`,
            subMenuProps: {
               items: opsList
            }
         })
      }

      menuProps.items.push({
         key: 'jobstep_history',
         text: 'Step History',
         onClick: () => setLogHistory(true)
      })


   }

   function updateError() {
      if (!handler || handler.currentError === undefined) return;
      handler.currentWarning = undefined;
      handler.stopTrailing();
      let lineIdx = errors[handler.currentError].lineIndex;
      const search = new URLSearchParams(window.location.search);
      search.set("lineindex", (lineIdx).toString());
      const url = `${window.location.pathname}?` + search.toString();               
      lineIdx -= 10;
      if (lineIdx < 0) {
         lineIdx = 0;
      }
      handler.externalUpdate();
      listRef?.scrollToIndex(lineIdx, () => handler.lineHeight, ScrollToMode.top);

      navigate(url, { replace: true })
   }

   function prevError() {
      if (!handler || !errors.length) return;

      if (handler.currentError === undefined) {
         handler.currentError = errors.length - 1;
      } else {
         handler.currentError--;
         if (handler.currentError < 0) {
            handler.currentError = errors.length - 1;
         }
      }
      updateError();
   }

   function nextError() {
      if (!handler) return;

      if (handler.currentError === undefined) {
         handler.currentError = 0;
      } else {
         handler.currentError++;
         handler.currentError %= errors.length;
      }

      updateError();
   }

   function updateWarning() {
      if (!handler || handler.currentWarning === undefined) return;

      handler.currentError = undefined;

      handler.stopTrailing();
      let lineIdx = warnings[handler.currentWarning].lineIndex ;
      const search = new URLSearchParams(window.location.search);
      search.set("lineindex", (lineIdx).toString());
      const url = `${window.location.pathname}?` + search.toString();               

      lineIdx -= 10
      if (lineIdx < 0) {
         lineIdx = 0;
      }
      handler.externalUpdate();
      listRef?.scrollToIndex(lineIdx, () => handler.lineHeight, ScrollToMode.top);

      navigate(url, { replace: true })
   }

   function prevWarning() {

      if (!handler || !warnings.length) return;

      if (handler.currentWarning === undefined) {
         handler.currentWarning = warnings.length - 1;
      } else {
         handler.currentWarning--;
         if (handler.currentWarning < 0) {
            handler.currentWarning = warnings.length - 1;
         }
      }

      updateWarning();
   }

   function nextWarning() {
      if (!handler) return;

      if (handler.currentWarning === undefined) {
         handler.currentWarning = 0;
      } else {
         handler.currentWarning++;
         handler.currentWarning %= warnings.length;
      }
      updateWarning();
   }

   let warningsText = "";
   let errorText = "";

   if (warnings?.length) {
      warningsText = warnings.length.toString();
      if (warnings.length >= 49) {
         warningsText += "+";
      }
   } else {
      warningsText = "No";
   }

   if (errors?.length) {
      errorText = errors.length.toString();
      if (errors.length >= 49) {
         errorText += "+";
      }
   } else {
      errorText = "No";
   }

   const buttonNoneText = dashboard.darktheme ? "#949898" : "#616e85";

   return <Stack>
      {!!fixme && <IssueModalV2 issueId={query.get("issue")} popHistoryOnClose={issueHistory} />}
      {!!fixme && logHistory && <StepHistoryModal jobDetails={fixme!} stepId={fixme!.stepByLogId(logId)?.id} onClose={() => setLogHistory(false)} />}
      {!!fixme && logArtifacts === "legacy" && <StepArtifactsModal jobDetails={fixme!} stepId={fixme!.stepByLogId(logId)?.id} onClose={() => setLogArtifacts("")} />}
      {!!fixme && !!artifactContext && logArtifacts !== "legacy" && <JobArtifactsModal jobId={fixme!.jobdata!.id} stepId={fixme!.stepByLogId(logId)?.id!} artifacts={(logSource as JobLogSource).artifactsV2} contextType={artifactContext} artifactPath={artifactPath} onClose={() => { navigate(window.location.pathname, { replace: true }) }} />}
      {!!historyAgentId && <HistoryModal agentId={historyAgentId} onDismiss={() => { navigate(baseUrl, { replace: true }); setHistoryAgentId(undefined) }} />}
      <Breadcrumbs items={logSource?.crumbs ?? []} title={logSource?.crumbTitle} />
      <Stack tokens={{ childrenGap: 0 }} style={{ backgroundColor: hordeTheme.horde.contentBackground, paddingTop: 12 }}>
         <Stack horizontal >
            <div key={`windowsize_logview1_${windowSize.width}_${windowSize.height}`} style={{ width: vw / 2 - (1440 / 2), flexShrink: 0 }} />
            <Stack tokens={{ childrenGap: 0, maxWidth: 1440 }} disableShrink={true} styles={{ root: { width: "100%", backgroundColor: hordeTheme.horde.contentBackground, paddingLeft: 4, paddingRight: 24, paddingTop: 12 } }}>
               <Stack horizontal style={{ paddingBottom: 4 }}>
                  <Stack className={hordeClasses.button} horizontal horizontalAlign={"start"} verticalAlign="center" tokens={{ childrenGap: 8 }}>
                     <Stack horizontal tokens={{ childrenGap: 2 }}>
                        <DefaultButton disabled={!errors.length} className={errors.length ? handler.style.errorButton : handler.style.errorButtonDisabled}
                           text={`${errorText} ${errors.length === 1 ? "Error" : "Errors"}`}
                           onClick={() => {
                              nextError();
                           }}
                           style={{ color: errors.length ? "#F9F9FB" : buttonNoneText, padding: 15 }} >
                           {!!errors.length && <Icon style={{ fontSize: 19, paddingLeft: 12 }} iconName='ChevronDown' />}
                        </DefaultButton>

                        {!!errors.length && <Stack>
                           <IconButton className={handler.style.errorButton} style={{ height: 30, fontSize: 19, padding: 8 }} iconProps={{ iconName: 'ChevronUp' }} onClick={(event: any) => {
                              event?.stopPropagation();
                              prevError();

                           }} />

                        </Stack>}


                        <Stack horizontal style={{ paddingLeft: 12 }} tokens={{ childrenGap: 2 }}>
                           <DefaultButton disabled={!warnings.length} className={warnings.length ? handler.style.warningButton : handler.style.warningButtonDisabled}
                              text={`${warningsText} ${warnings.length === 1 ? "Warning" : "Warnings"}`}
                              onClick={() => {
                                 nextWarning();
                              }}
                              style={{ color: (dashboard.darktheme && warnings.length) ? "#F9F9FB" : buttonNoneText, padding: 15 }} >
                              {!!warnings.length && <Icon style={{ fontSize: 19, paddingLeft: 12 }} iconName='ChevronDown' />}
                           </DefaultButton>
                           {!!warnings.length && <Stack>
                              <IconButton className={handler.style.warningButton} style={{ height: 30, fontSize: 19, padding: 8, color: (dashboard.darktheme && warnings.length) ? "#F9F9FB" : buttonNoneText }} iconProps={{ iconName: 'ChevronUp' }} onClick={(event: any) => {
                                 event?.stopPropagation();
                                 prevWarning();

                              }} />

                           </Stack>}
                        </Stack>


                        {!!menuProps.items.find(i => !i.disabled) && <Stack style={{ paddingLeft: 18 }}><PrimaryButton disabled={!menuProps.items.find(i => !i.disabled)} text="View" menuProps={menuProps} style={{ fontFamily: "Horde Open Sans SemiBold", borderStyle: "hidden", padding: 15 }} /></Stack>}
                        {!menuProps.items.find(i => !i.disabled) && <Stack style={{ paddingLeft: 18 }}><DefaultButton disabled={true} className={handler.style.warningButtonDisabled} text="View" style={{ color: "rgb(97, 110, 133)", padding: 15 }} /> </Stack>}

                     </Stack>
                     <Stack>
                        {logSource.active && <LogProgressIndicator logSource={logSource} />}
                     </Stack>
                  </Stack>

                  <Stack grow />
                  <Stack horizontalAlign={"end"}>
                     <Stack>
                        <Stack verticalAlign="center" horizontal tokens={{ childrenGap: 24 }} styles={{ root: { paddingRight: 4, paddingTop: 4 } }}>
                           <Stack horizontal tokens={{ childrenGap: 24 }}>
                              <Stack horizontal tokens={{ childrenGap: 0 }}>
                                 {!!searchState.curRequest && <Spinner style={{ paddingRight: 8 }} />}

                                 <TextField
                                    suffix={searchState?.results?.length ? `${curSearchIdx + 1}/${searchState?.results?.length}` : undefined}
                                    spellCheck={false}
                                    autoComplete="off"
                                    deferredValidationTime={1000}
                                    validateOnLoad={false}
                                    componentRef={searchBox}

                                    styles={{
                                       root: { width: 280, fontSize: 12 }, fieldGroup: {
                                          borderWidth: 1
                                       }
                                    }}
                                    placeholder="Search"

                                    onKeyPress={(ev) => {

                                       if (ev.key === "Enter" && !searchState.curRequest && !inTimeout) {
                                          if (searchBox.current?.value === searchState.search) {
                                             searchDown();
                                          } else {
                                             doQuery(searchBox.current?.value ?? "");
                                          }
                                       }

                                    }}

                                    onGetErrorMessage={(newValue) => {

                                       doQuery(newValue);
                                       return undefined;


                                    }}

                                 />
                                 <Stack horizontal style={{ borderWidth: 1, borderStyle: "solid", borderColor: dashboard.darktheme ? "#3F3F3F" : "rgb(96, 94, 92)", height: 32, borderLeft: 0 }}>
                                    <IconButton style={{ height: 30 }} iconProps={{ iconName: 'ChevronUp' }} onClick={(event: any) => {
                                       searchUp();
                                    }} />
                                    <IconButton style={{ height: 30 }} iconProps={{ iconName: 'ChevronDown' }} onClick={(event: any) => {
                                       searchDown();
                                    }} />

                                 </Stack>
                              </Stack>

                              <Dropdown
                                 styles={{ root: { width: 92 } }}
                                 options={[{ key: 'relative', text: 'Relative' }, { key: 'local', text: 'Local' }, { key: 'utc', text: 'UTC' }]}
                                 defaultSelectedKey={tsFormat}
                                 onChanged={(value) => {
                                    setTSFormat(value.key as string);
                                    listRef?.forceUpdate();
                                 }}
                              />

                              <PrimaryButton
                                 text="Download"
                                 split
                                 onClick={() => logSource.download(false)}
                                 menuProps={downloadProps}
                                 style={{ fontFamily: "Horde Open Sans SemiBold" }}
                              />
                           </Stack>
                        </Stack>
                     </Stack>
                  </Stack>

               </Stack>
               <Stack>
                  <Stack style={{ paddingTop: 12 }}>
                     <Separator styles={{ root: { fontSize: 0, width: "100%", padding: 0, selectors: { '::before': { background: dashboard.darktheme ? '#313638' : '#D3D2D1' } } } }} />
                  </Stack>

                  <Stack style={{ paddingBottom: 12, paddingTop: 12, fontSize: 12, fontFamily: "Horde Open Sans Regular" }}>
                     <Stack style={{ paddingLeft: 4 }} horizontal verticalAlign="center" tokens={{ childrenGap: 12 }}>
                        <Text style={{ fontSize: 12, fontFamily: "Horde Open Sans Semibold" }}>Summary:</Text>
                        <Markdown>{summaryText}</Markdown>
                     </Stack>
                  </Stack>
                  <Stack horizontalAlign="center" style={{ paddingBottom: 12 }}>
                     <Separator styles={{ root: { fontSize: 0, width: "100%", padding: 0, selectors: { '::before': { background: dashboard.darktheme ? '#313638' : '#D3D2D1' } } } }} />
                  </Stack>
               </Stack>

            </Stack>
         </Stack>

         <Stack style={{ backgroundColor: hordeTheme.horde.contentBackground, paddingLeft: "24px", paddingRight: "24px" }}>
            <Stack tokens={{ childrenGap: 0 }}>
               <FocusZone direction={FocusZoneDirection.vertical} isInnerZoneKeystroke={() => { return true; }} defaultActiveElement="#LogList" style={{ padding: 0, margin: 0 }} >
                  <div className={handler.style.container} data-is-scrollable={true}
                     onScroll={(ev) => {

                        const element: any = ev.target;

                        const scroll = Math.ceil(element.scrollHeight - element.scrollTop);

                        if (scroll < (element.clientHeight + 1)) {
                           handler.scroll = undefined;
                           handler.trailing = true;
                        } else {

                           if (handler.scroll === undefined) {
                              handler.scroll = scroll;
                           } else if (handler.scroll !== undefined && (handler.scroll < scroll || scroll > (element.clientHeight + 1))) {
                              handler.stopTrailing();
                           }
                        }


                     }}>
                     <Stack horizontal>
                        {!dashboard.leftAlignLog && <div key={`windowsize_logview2_${windowSize.width}_${windowSize.height}`} style={{ width: vw / 2 - (1440 / 2) - 48, flexShrink: 0 }} />}
                        <Stack styles={{ root: { "backgroundColor": hordeTheme.horde.contentBackground, paddingLeft: "0px", paddingRight: "0px" } }}>
                           <SelectionZone selection={selection} selectionMode={SelectionMode.multiple}>
                              <List key={`log_list_key_${logListKey}`} id="LogList" ref={(list: List) => { listRef = list; }}
                                 items={logSource.logItems}
                                 // NOTE: getPageSpecification breaks initial scrollToIndex when query contains lineIndex!
                                 getPageHeight={() => 10 * (handler.lineHeight)}
                                 onShouldVirtualize={() => { return true; }}
                                 onRenderCell={onRenderCell}
                                 onPagesUpdated={() => {
                                    if (handler.initialRender && listRef) {
                                       handler.initialRender = false;
                                       if (logSource?.startLine !== undefined) {
                                          listRef.scrollToIndex(logSource.startLine - 1, () => handler.lineHeight, ScrollToMode.center);
                                       } else if (handler.trailing) {
                                          listRef.scrollToIndex(logSource.logData!.lineCount - 1, undefined, ScrollToMode.bottom);
                                       }
                                    }
                                 }}

                                 data-is-focusable={true} />
                           </SelectionZone>
                        </Stack>
                     </Stack>
                  </div>
               </FocusZone>
            </Stack>
         </Stack>

      </Stack>
   </Stack>;

});

export const LogView: React.FC = () => {

   const { logId } = useParams<{ logId: string }>();

   useEffect(() => {
      return () => {
         selection.setItems([], true);
         LogHandler.clear();
      };
   }, []);

   const { hordeClasses } = getHordeStyling();


   if (!logId) {
      console.error("Bad log id to LogView");
   }

   return (
      <Stack className={hordeClasses.horde}>
         <TopNav />
         <LogList logId={logId!} />
      </Stack>
   );
};