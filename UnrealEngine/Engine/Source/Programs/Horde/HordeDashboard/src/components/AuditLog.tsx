// Copyright Epic Games, Inc. All Rights Reserved.
import { ComboBox, DefaultButton, Dropdown, FocusZone, FocusZoneDirection, IDropdownOption, List, mergeStyleSets, SearchBox, Selection, SelectionMode, SelectionZone, Spinner, SpinnerSize, Stack, Text } from "@fluentui/react";
import { action, makeObservable, observable } from 'mobx';
import { observer } from "mobx-react-lite";
import moment from "moment";
import { useEffect, useState } from "react";
import Highlight from "react-highlighter";
import { useParams } from "react-router";
import { Link } from "react-router-dom";
import backend from "../backend";
import { AuditLogEntry } from "../backend/Api";
import dashboard from "../backend/Dashboard";
import { useWindowSize } from "../base/utilities/hooks";
import { displayTimeZone } from "../base/utilities/timeUtils";
import { BreadcrumbItem, Breadcrumbs } from "./Breadcrumbs";
import { DateTimeRange } from "./DateTimeRange";
import { HistoryModal } from "./HistoryModal";
import { IssueModalV2 } from "./IssueViewV2";
import { getLogStyles, logMetricNormal } from "./LogStyle";
import { TopNav } from "./TopNav";
import { getHordeStyling } from "../styles/Styles";
import { getHordeTheme } from "../styles/theme";


let _auditStyleNormal: any;

const getAuditStyleNormal = () => {

   const theme = getHordeTheme();
   const { logStyleNormal } = getLogStyles();

   const auditStyleNormal = _auditStyleNormal ?? mergeStyleSets(logStyleNormal, {

      logLine: [
          {
            selectors: {
               '&:hover': { background: theme.palette.neutralLight }
             },
          }
      ]
   });

   _auditStyleNormal = auditStyleNormal;

   return auditStyleNormal;  
}



class AuditLogHandler {

   constructor() {
      makeObservable(this);
      this.clear();
   }


   clear() {

      this.agentId = undefined;
      this.issueId = undefined;
      
      this.timeSelectKey = "time_live_tail";

      clearTimeout(this.timeoutId);
      this.timeoutId = undefined;

      // cancel any pending        
      for (let i = 0; i < this.cancelId; i++) {
         this.canceled.add(i);
      }

      this.updating = false;
      this.haveUpdated = false;

   }

   get tailing(): boolean {
      return this.timeSelectKey === "time_live_tail";
   }

   setAgent(agentId?: string) {

      if (!agentId) {
         this.clear();
         return;
      }

      if (this.agentId === agentId) {
         return;
      }

      this.agentId = agentId;
      this.update();
   }

   setIssue(issueId?: string) {

      if (!issueId) {
         this.clear();
         return;
      }

      if (this.issueId === issueId) {
         return;
      }

      this.issueId = issueId;
      this.update();
   }


   get isAgentLog():boolean {
      return !!this.agentId;
   }

   get isIssueLog():boolean {
      return !!this.issueId;
   }

   async update() {

      clearTimeout(this.timeoutId);

      if (this.tailing) {

         this.timeoutId = setTimeout(() => { this.update(); }, this.pollTime);

         if (this.updating) {
            return;
         }
   
      }

      // cancel any pending        
      for (let i = 0; i < this.cancelId; i++) {
         this.canceled.add(i);
      }

      if (!this.agentId && !this.issueId) {
         return;
      }


      try {

         this.updating = true;

         const cancelId = this.cancelId++;

         let minTime = this.minDate;
         let maxTime = this.maxDate;

         if (this.tailing) {
            
            if (this.issueId) {
               minTime = undefined;
            } else if (this.agentId) {
               // live tail of the last 4 days
               // We need to optimize the agent history endpoint, this was an attempt to limit results though doesn't work 
               minTime = undefined; //new Date(new Date().valueOf() - (60 * 24 * 4 * 60000));
            }
            
            maxTime = new Date();
         }

         let entries: AuditLogEntry[] = [];

         if (this.agentId) {
            entries = await backend.getAgentHistory(this.agentId, { minTime: minTime?.toISOString(), maxTime: maxTime?.toISOString(), count: 8192 * 2 });
         }
         
         if (this.issueId) {
            entries = await backend.getIssueHistory(this.issueId, { minTime: minTime?.toISOString(), maxTime: maxTime?.toISOString(), count: 8192 * 2 });
         }

         // check for canceled during graph request
         if (this.canceled.has(cancelId)) {
            return;
         }

         this.entries = entries;

         this.haveUpdated = true;

         this.setUpdated();

      } catch (reason) {

         console.error(reason);

      } finally {

         this.updating = false;
      }

   }

   setTimeSelection(time: TimeSelection) {

      const live = time.key === "time_live_tail";
      const calendar = time.key === "time_select_calendar";

      if (calendar) {
         console.error("Should not be setting time_select_calendar in setTimeSelection");
      }

      // @todo
      if (live) {
         this.timeSelectKey = time.key;
         this.minDate = this.maxDate = undefined;
         this.haveUpdated = false;
         this.update();
         this.setUpdated();
         return;
      }

      this.minDate = new Date(new Date().valueOf() - (time.minutes * 60000));
      this.maxDate = new Date();
      this.timeSelectKey = time.key;
      this.haveUpdated = false;
      this.update();
      this.setUpdated();

   }

   setTimeRange(minDate: Date, maxDate: Date) {

      this.minDate = minDate;
      this.maxDate = maxDate;

      this.timeSelectKey = "time_select_calendar";

      this.haveUpdated = false;
      this.update();

      this.setUpdated();

   }

   agentId?: string;
   issueId?: string;

   timeSelectKey?: string;

   minDate?: Date;
   maxDate?: Date;

   scroll?: number;

   entries: AuditLogEntry[] = [];

   haveUpdated?: boolean;

   @action
   setUpdated() {
      this.updated++;
   }

   @observable
   updated: number = 0;

   updating = false;
   private timeoutId: any;

   private canceled = new Set<number>();
   private cancelId = 0;

   private pollTime = 15000;

}

const handler = new AuditLogHandler();

type TimeSelection = {
   text: string;
   key: string;
   minutes: number;
}

const timeSelections: TimeSelection[] = [
   {
      text: "Live Tail", key: "time_live_tail", minutes: 0
   },
   {
      text: "Past 15 Minutes", key: "time_15_minutes", minutes: 15
   },
   {
      text: "Past 1 Hour", key: "time_1_hour", minutes: 60
   },
   {
      text: "Past 4 Hours", key: "time_4_hours", minutes: 60 * 4
   },
   {
      text: "Past 1 Day", key: "time_1_day", minutes: 60 * 24
   },
   {
      text: "Past 2 Days", key: "time_2_days", minutes: 60 * 24 * 2
   },
   {
      text: "Past 1 Week", key: "time_1_week", minutes: 60 * 24 * 7
   },
   {
      text: "Past 1 Month", key: "time_1_month", minutes: 60 * 24 * 31 // yeah, not all months have 31 days, sheesh 
   },
   {
      text: "Select from Calendar", key: "time_select_calendar", minutes: 0
   }
]

const selection = new Selection({ selectionMode: SelectionMode.multiple });

export const AuditLogPanel: React.FC<{ agentId?: string, issueId?: string }> = observer(({ agentId, issueId }) => {

   const windowSize = useWindowSize();
   const [showDatePicker, setShowDatePicker] = useState(false);
   const [tsFormat, setTSFormat] = useState(dashboard.displayUTC ? 'UTC' : 'Local');
   const [search, setSearch] = useState<string | undefined>(undefined);
   const [selectedTypes, setSelectedTypes] = useState<string[]>(["All"]);
   const [viewAgent, setViewAgent] = useState(false);
   const [viewIssue, setViewIssue] = useState(false);


   useEffect(() => {

      return () => {
         handler.clear();
      };

   }, []);

   const auditStyleNormal = getAuditStyleNormal();
   const { modeColors } = getHordeStyling();

   if (handler.updated) { }

   if (agentId && handler.agentId !== agentId) {
      handler.setAgent(agentId);
   }

   if (issueId && handler.issueId !== issueId) {
      handler.setIssue(issueId);
   }

   if (!agentId && !issueId) {
      return null;
   }

   const vw = Math.max(document.documentElement.clientWidth, window.innerWidth || 0);

   type AuditLogItem = {
      entry: AuditLogEntry;
      type: string;
   }

   const searchTerm = search?.toLowerCase();

   const entryTypes: Set<string> = new Set();

   let logItems: AuditLogItem[] = handler.entries.map((entry) => {

      let type = entry.level.toString();
      if (entry.properties && entry.properties["Type"]) {
         type = entry.properties["Type"] as string;
      } else {
         // @todo: 
         if (entry.message.startsWith("Compute")) {
            type = "Compute";
         }
      }

      entryTypes.add(type);

      return { entry: entry, type: type }

   })

   // filter
   logItems = logItems.filter(item => {

      if (selectedTypes.indexOf("All") === -1) {
         if (selectedTypes.indexOf(item.type) === -1) {
            return false;
         }
      }

      if (!searchTerm) {
         return true;
      }

      if (item.entry.message.toLowerCase().indexOf(searchTerm) === -1) {
         return false;
      }

      return true;

   });

   const typeItems: IDropdownOption[] = Array.from(entryTypes.keys()).map(type => {
      return {
         key: type,
         text: type
      };
   }).sort((a, b) => { if (a.text === b.text) return 0; return a.text < b.text ? -1 : 1 });

   typeItems.unshift({
      key: "All",
      text: "All"
   })

   let crumbItems: BreadcrumbItem[] = [];
   if (agentId) {
      crumbItems = [
         {
            text: `Agents`,
            link: '/agents'
         },
         {
            text: `Agent Audit - ${agentId}`,
            link: `/agents?agentId=${encodeURIComponent(agentId)}`
         }
      ];

   }

   if (issueId) {
      crumbItems = [
         {
            text: `Issues`,
         },
         {
            text: `Audit - Issue ${issueId}`,
         }
      ];

   }


   const onRenderCell = (item?: AuditLogItem, index?: number, isScrolling?: boolean): JSX.Element => {
      const entry = item!.entry;

      let tm = moment.utc(entry.time);
      if (tsFormat === 'Local') {
         tm = tm.local();
      }

      const format = dashboard.display24HourClock ? "MMM DD HH:mm:ss" : "MMM DD hh:mm:ss A";

      let timestamp = `[${tm.format(format)}]`;

      return (
            <Stack className={auditStyleNormal.logLine} key={`key_log_line_${item?.entry.time}`} style={{ width: "100%", height: logMetricNormal.lineHeight }}>
               <div style={{ position: "relative" }}>
                  <Stack tokens={{ childrenGap: 8 }} horizontal disableShrink={true}>
                     <Stack horizontal disableShrink={true} >
                        <Stack styles={{ root: { width: 140, whiteSpace: "nowrap", fontSize: logMetricNormal.fontSize, userSelect: "none" } }}> {timestamp}</Stack>
                        <Stack styles={{ root: { color: "#8a8a8a", width: 92, paddingRight: 8, whiteSpace: "nowrap", textAlign: "right", fontSize: logMetricNormal.fontSize, userSelect: "none" } }}> [{item!.type}]</Stack>
                        <div className={auditStyleNormal.logLineOuter}> <Stack styles={{ root: { paddingLeft: 8, paddingRight: 8 } }}> {renderAuditEntry(entry, search)}</Stack></div>
                     </Stack>
                  </Stack>
               </div>
            </Stack>
      );
   }

   let timeComboText: string | undefined;
   let timeComboWidth = 180;

   if (handler.timeSelectKey === "time_select_calendar") {

      let format = dashboard.display24HourClock ? "MMM DD H:mm z" : "MMM DD h:mm A z";
      timeComboText = moment(handler.minDate).tz(displayTimeZone()).format(format);
      timeComboText += " / " + moment(handler.maxDate).tz(displayTimeZone()).format(format);
      timeComboWidth = 350;
   }

   return <Stack>
      <Breadcrumbs items={crumbItems} />
      <Stack tokens={{ childrenGap: 12 }}>
         <Stack horizontal>
            <div key={`windowsize_logview1_${windowSize.width}_${windowSize.height}`} style={{ width: vw / 2 - (1440/2) - 230, flexShrink: 0, backgroundColor: modeColors.background }} />
            <Stack tokens={{ childrenGap: 0 }} styles={{ root: { backgroundColor: modeColors.background, margin: "auto", paddingTop: 12, paddingRight: 10 } }}>
               <Stack horizontal styles={{ root: { paddingLeft: 0, paddingBottom: 4, paddingRight: 12, width: 1440 } }}>
                  <Stack horizontal tokens={{ childrenGap: 12 }}>
                     <Stack>
                        <SearchBox style={{ width: 400 }} onEscape={() => setSearch(undefined)} autoComplete="off" disableAnimation={true} spellCheck={false} onChange={(ev, value) => setSearch(value)} />
                     </Stack>
                     <Stack>
                        <Dropdown style={{ width: 240 }} options={typeItems} multiSelect selectedKeys={selectedTypes}
                           onChange={(event, option, index) => {

                              if (option) {

                                 let filter = [...selectedTypes];
                                 if (!option.selected) {
                                    filter = filter.filter(k => k !== option.key);
                                 } else {
                                    if (filter.indexOf(option.key as string) === -1) {
                                       filter.push(option.key as string);
                                    }
                                 }

                                 if (!filter.length || (option.selected && option.key === "All")) {
                                    filter = ["All"];
                                 }

                                 if (filter.find(k => k === "All") && filter.length > 1) {
                                    filter = filter.filter(k => k !== "All");
                                 }

                                 setSelectedTypes(filter);
                              }
                           }}
                        />
                     </Stack>
                  </Stack>

                  <Stack grow />
                  <Stack horizontalAlign={"end"}>
                     <Stack>
                        <Stack verticalAlign="center" horizontal tokens={{ childrenGap: 24 }} styles={{ root: { paddingRight: 4, paddingTop: 4 } }}>
                           <Stack horizontal tokens={{ childrenGap: 24 }}>
                              <Stack>
                                 <ComboBox
                                    styles={{ root: { width: timeComboWidth } }}
                                    options={timeSelections}
                                    text={timeComboText}
                                    selectedKey={handler.timeSelectKey}
                                    onItemClick={(ev, option, index) => {
                                       if (!option) {
                                          return;
                                       }

                                       if (option.key === "time_select_calendar") {
                                          setShowDatePicker(true);
                                       }
                                    }}
                                    onChange={(ev, option, index, value) => {
                                       const select = option as TimeSelection;

                                       if (select.key === "time_select_calendar") {

                                       } else {
                                          handler.setTimeSelection(select);
                                       }

                                    }}
                                 />
                              </Stack>
                              <Stack>
                                 <Dropdown
                                    styles={{ root: { width: 92 } }}
                                    options={[{ key: 'Local', text: 'Local' }, { key: 'UTC', text: 'UTC' }]}
                                    defaultSelectedKey={tsFormat}
                                    onChanged={(value) => {
                                       setTSFormat(value.key as string);
                                       // Audit fix me
                                       //listRef?.forceUpdate();
                                    }}
                                 />
                              </Stack>
                              <Stack>
                                 {handler.isAgentLog && <DefaultButton style={{ fontFamily: "Horde Open Sans SemiBold" }} text="View Agent" onClick={() => setViewAgent(true)} />}
                                 {handler.isIssueLog && <DefaultButton style={{ fontFamily: "Horde Open Sans SemiBold" }} text="View Issue" onClick={() => setViewIssue(true)} />}
                              </Stack>

                           </Stack>
                        </Stack>
                     </Stack>
                  </Stack>

               </Stack>
            </Stack>
         </Stack>

         <Stack style={{ backgroundColor: modeColors.background, paddingLeft: "24px", paddingRight: "24px" }}>
            <Stack tokens={{ childrenGap: 0 }}>
               {showDatePicker && < DateTimeRange onChange={(minDate, maxDate) => { handler.setTimeRange(minDate, maxDate); setShowDatePicker(false) }} onDismiss={() => { handler.setTimeSelection({ text: "Live Tail", key: "time_live_tail", minutes: 0 }); setShowDatePicker(false) }} />}
               {viewAgent && <HistoryModal agentId={agentId} onDismiss={() => { setViewAgent(false) }} />}
               {viewIssue && <IssueModalV2 issueId={issueId} onCloseExternal={() => { setViewIssue(false);} } popHistoryOnClose={false} />}
               <FocusZone direction={FocusZoneDirection.vertical} isInnerZoneKeystroke={() => { return true; }} defaultActiveElement="#LogList" style={{ padding: 0, margin: 0 }} >
                  <div className={auditStyleNormal.container} style={{ height: 'calc(100vh - 260px)', position: 'relative' }} data-is-scrollable={true}>
                     <Stack horizontal>
                        <div key={`windowsize_logview2_${windowSize.width}_${windowSize.height}`} style={{ width: vw / 2 - (1440/2) - 24, flexShrink: 0 }} />
                        <Stack styles={{ root: { backgroundColor: modeColors.background, paddingLeft: "0px", paddingRight: "0px" } }}>
                           {!handler.haveUpdated && <Spinner size={SpinnerSize.large} />}
                           {!!handler.haveUpdated && !logItems.length && <Stack style={{ paddingLeft: 0 }}><Text variant="mediumPlus">No audit entries found</Text></Stack>}

                           {!!handler.haveUpdated && !!logItems.length &&
                              <SelectionZone selection={selection} selectionMode={SelectionMode.multiple}>
                                 <List key={`audit_log_list_key`} id="LogList"
                                    items={logItems}
                                    // NOTE: getPageSpecification breaks initial scrollToIndex when query contains lineIndex!
                                    getPageHeight={() => 10 * (logMetricNormal.lineHeight)}
                                    onShouldVirtualize={() => { return true; }}
                                    onRenderCell={onRenderCell}
                                    data-is-focusable={true} />
                              </SelectionZone>
                           }
                        </Stack>
                     </Stack>
                  </div>
               </FocusZone>
            </Stack>
         </Stack>

      </Stack>
   </Stack>

});

export const AuditLogView: React.FC = () => {

   const { hordeClasses } = getHordeStyling();

   const { agentId, issueId } = useParams<{ agentId: string, issueId: string }>();
   return <Stack className={hordeClasses.horde}>
      <TopNav />
      <Stack>
         {!!agentId && <AuditLogPanel agentId={agentId} />}
         {!!issueId && <AuditLogPanel issueId={issueId} />}
      </Stack>
   </Stack>

}

// Audit log rendering generally means creating a link from a tag 
// some tags depend on other tags to generate the route includig query parameters, and need to have knowledge of the routes themselves,
// which means can be somewhat data/tag driven, though not entirely

// @todo: We probably want to be able to register subclass constructors with AuditLine, to dynamically add... or use a generic.  
enum AuditLogType {
   Unknown,
   Agent,
   Issue
}

type AuditProperty = {
   tag: string; // without enclosing {}
   value: string | number;
   type?: string;
}

class AuditLine {

   protected constructor(logType: AuditLogType, entry: AuditLogEntry) {

      this.logType = logType;
      this.entry = entry;

   }

   static create(logType: AuditLogType, entry: AuditLogEntry, search?: string) {

      let instance: AuditLine | undefined;

      if (logType === AuditLogType.Agent) {
         instance = new AgentAuditLine(logType, entry);
      }

      if (logType === AuditLogType.Issue) {
         instance = new IssueAuditLine(logType, entry);
      }

      if (instance) {
         instance.search = search;
         instance.process();
      }

      return instance;

   }

   private process() {

      const entry = this.entry;
      const properties = entry.properties;

      const match = entry.format.match(AuditLine.tagRegex)?.map(m => {
         m = m.replaceAll("{", "");
         m = m.replaceAll("}", "");
         return m;
      });

      if (!match?.length || !properties) {
         return;
      }

      match.forEach(m => {

         let property = properties[m];

         if (property === null) {

            this.properties.set(m, {
               tag: m,
               value: "null",
            })

            return;

         }

         if (typeof (property) === "string" || typeof (property) === "number" || typeof(property) === "boolean") {

            this.properties.set(m, {
               tag: m,
               value: property as any,
            })

            return;
         }

         const record = property as Record<string, string | number>;

         const type = record["$type"] as string | undefined;
         const value = record["$text"];

         this.properties.set(m, {
            tag: m,
            value: value,
            type: type ? type : undefined
         })
      });

   }

   protected highlight(key: string, text: string) {
      return <Highlight key={`${key}_hightlight`} search={this.search ? this.search : ""} >{text}</Highlight>
   }


   renderLine(): JSX.Element | null {

      const entry = this.entry;
      const format = entry.format;

      let tags: string[] = [];

      const match = format.match(AuditLine.tagRegex)?.map(m => {
         return m;
      });

      if (match?.length) {
         tags = match;
      }

      let renderedTags = tags.map((tag, index) => {

         tag = tag.replaceAll("{", "");
         tag = tag.replaceAll("}", "");

         const key = `audit_entry_${index}`;
         const property = this.properties.get(tag);

         if (!property) {
            return <span>{this.highlight(key, "!==> Missing Property <==!")}</span>;
         }

         // Common recognizers

         let element: JSX.Element | null = null;

         // @todo: switch these over to use types, with fallback to tag

         // LeaseId
         if (tag === "LeaseId") {

            const leaseId = property.value as string;
            const logId = this.properties.get("LogId")?.value;

            if (logId && leaseId) {
               let to = `/log/${logId}`;
               element = <Link to={to} >{this.highlight(key, leaseId)}</Link>
            }

         }

         // LogId
         if (tag === "LogId") {

            const logId = property.value as string;            

            if (logId) {
               let to = `/log/${logId}`;
               element = <Link to={to} >{this.highlight(key, logId)}</Link>
            }

         }


         // JobId
         if (tag === "JobId") {

            const jobId = property.value as string;

            if (jobId) {
               let to = `/job/${jobId}`;
               element = <Link to={to} >{this.highlight(key, jobId)}</Link>
            }

         }

         // BatchId
         if (tag === "BatchId") {

            const batchId = property.value as string;
            const jobId = this.properties.get("JobId")?.value;

            if (batchId && jobId) {
               let to = `/job/${jobId}?batch=${batchId}`;
               element = <Link to={to} >{this.highlight(key, batchId)}</Link>
            }

         }

         // Change
         if (tag === "Change") {

            const change = property.value?.toString();

            if (change) {
               let to = `${dashboard.swarmUrl}/changes/${change}`;
               element = <a href={to} rel="noreferrer" target="_blank" >{this.highlight(key, change)}</a>
            }

         }

         if (element) {
            return element;
         }

         return <span>{this.highlight(key, property.value?.toString())}</span>;


      });

      let remaining = entry.format;

      renderedTags = renderedTags.map((t, idx) => {

         let current = remaining;

         const tag = tags[idx];
         const index = remaining.indexOf(tag);

         remaining = remaining.slice(tag.length + (index > 0 ? index : 0));

         if (index < 0) {
            console.error("not able to find tag in format");
            return <Text>Error, unable to find tag</Text>;
         }

         if (index === 0) {
            return t;
         }

         const rtags = [];

         const key = `log_line_${idx}_${index}_fragment`;

         rtags.push(<Highlight key={key} search={this.search ? this.search : ""} >{current.slice(0, index)}</Highlight>);
         rtags.push(t);

         return rtags;

      }).flat();

      if (remaining) {
         const key = `log_line_remaining_remaining_fragment`;
         renderedTags.push(<Highlight key={key} search={this.search ? this.search : ""} >{remaining}</Highlight>)
      }

      return <div>
         {renderedTags}
      </div>;

   }

   entry: AuditLogEntry;

   properties: Map<string, AuditProperty> = new Map();

   logType = AuditLogType.Unknown;

   search?: string;

   private static tagRegex = /{[^{}]+}/g
}

class AgentAuditLine extends AuditLine {

}

class IssueAuditLine extends AuditLine {

}


// Line Rendering -------------------------------------------------------------------------------------------------------------------


const renderAuditEntry = (entry: AuditLogEntry, search?: string) => {

   let type = AuditLogType.Unknown;

   if (handler.isAgentLog) {
      type = AuditLogType.Agent;
   }

   if (handler.isIssueLog) {
      type = AuditLogType.Issue;
   }

   if (type === AuditLogType.Unknown) {
      return <div>Unknown Log Type</div>
   }

   const line = AuditLine.create(type, entry, search);
   if (!line) {
      return null;
   }

   return line.renderLine();
}

