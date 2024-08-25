import { Checkbox, DatePicker, DefaultButton, DetailsList, DetailsRow, IColumn, IconButton, IContextualMenuItem, IContextualMenuProps, IDetailsGroupRenderProps, IDetailsListProps, IGroup, Label, Modal, PrimaryButton, SelectionMode, Spinner, SpinnerSize, Stack, Text, TextField } from "@fluentui/react";
import { observer } from "mobx-react-lite";
import moment from "moment";
import { useState } from "react";
import { Link } from "react-router-dom";
import backend from "../backend";
import { GetJobResponse, GetJobsTabResponse, GetJobStepRefResponse, JobData, JobQuery, TabType, GetTemplateRefResponse } from "../backend/Api";
import dashboard from "../backend/Dashboard";
import { projectStore } from "../backend/ProjectStore";
import templateCache from '../backend/TemplateCache';
import { displayTimeZone } from "../base/utilities/timeUtils";
import { ChangeButton } from "./ChangeButton";
import { StepRefStatusIcon } from "./StatusIcon";
import { UserSelect } from "./UserSelect";
import { getHordeTheme } from "../styles/theme";
import { getHordeStyling } from "../styles/Styles";


const jobToStep = new Map<string, GetJobStepRefResponse>();
let jobStepName = "";

export const JobSearchSimpleModal: React.FC<{ streamId: string, onClose: () => void }> = observer(({ streamId, onClose }) => {

   type JobItem = {
      job: GetJobResponse
   };

   const [searchState, setSearchState] = useState<{ items: JobItem[], groups?: IGroup[], templateId?: string, name?: string, userId?: string, global?: boolean, streamIdOverride?: string, preflights: boolean, preflightOnly?: boolean, minDate?: Date, maxDate?: Date, minCL?: string, maxCL?: string, querying: boolean, containsStep?: string, containsParameter?: string }>({ items: [], querying: false, preflights: false });
   const [templateData, setTemplateData] = useState<{ streamId: string; templates: GetTemplateRefResponse[] } | undefined>(undefined);

   const { hordeClasses, detailClasses } = getHordeStyling();

   const stream = projectStore.streamById(streamId);

   if (!stream) {
      console.error("unable to get stream");
      return <div>unable to get stream</div>;
   }

   const hordeTheme = getHordeTheme();

   const StatusModal: React.FC<{ text: string }> = ({ text }) => {
      return <Modal isOpen={true} isBlocking={true} topOffsetFixed={true} styles={{ main: { padding: 8, width: 700, hasBeenOpened: false, top: "80px", position: "absolute" } }} className={hordeClasses.modal}>
         <Stack tokens={{ childrenGap: 40 }} styles={{ root: { padding: 8 } }}>
            <Stack grow verticalAlign="center">
               <Text variant="mediumPlus" styles={{ root: { fontWeight: "unset", fontFamily: "Horde Open Sans SemiBold" } }}>{text}</Text>
            </Stack>
            <Stack verticalAlign="center">
               <Spinner size={SpinnerSize.large} />
            </Stack>

         </Stack>
      </Modal>
   }

   // templates
   if (!templateData || streamId !== templateData.streamId) {
      templateCache.getStreamTemplates(stream).then(data => setTemplateData({ streamId: streamId!, templates: data }));
      return <StatusModal text={"Loading Templates"} />
   } else if (templateData.streamId !== streamId) {
      setTemplateData(undefined);
      return null;
   }

   let templates = templateData.templates;

   let templateOptions: IContextualMenuItem[] = [];


   const sorted = new Map<string, GetTemplateRefResponse[]>();

   templates.forEach(t => {

      stream.tabs.forEach(tab => {
         if (tab.type !== TabType.Jobs) {
            return;
         }

         const jtab = tab as GetJobsTabResponse;
         if (!jtab.templates?.find(template => template === t.id)) {
            return;
         }

         if (!sorted.has(jtab.title)) {
            sorted.set(jtab.title, []);
         }

         sorted.get(jtab.title)!.push(t);

      })
   })

   Array.from(sorted.keys()).sort((a, b) => a < b ? -1 : 1).forEach(cat => {

      const templates = sorted.get(cat);
      if (!templates?.length) {
         return;
      }

      const subItems = templates.sort((a, b) => a.name < b.name ? -1 : 1).map(t => {
         return { key: t.id, text: t.name, onClick: () => { setSearchState({ ...searchState, templateId: t.id }) } };
      })

      templateOptions.push({ key: `${cat}_category`, text: cat, subMenuProps: { items: subItems } });

   })

   templateOptions.push({ key: `show_all_templates`, text: "All Templates", onClick: (ev) => { ev?.stopPropagation(); ev?.preventDefault(); setSearchState({ ...searchState, templateId: undefined, containsParameter: undefined }) } });

   const templateMenuProps: IContextualMenuProps = {
      shouldFocusOnMount: true,
      subMenuHoverDelay: 0,
      items: templateOptions,
   };


   const template = templates.find(t => t.id === searchState.templateId);

   // results
   const columns = [
      { key: 'column1', name: 'Change', minWidth: 60, maxWidth: 60, isResizable: false },
      { key: 'column2', name: 'Name', minWidth: 340, maxWidth: 340, isResizable: false },
      { key: 'column3', name: 'Step', minWidth: 200, maxWidth: 200, isResizable: false },
      { key: 'column4', name: 'StartedBy', minWidth: 120, maxWidth: 120, isResizable: false },
      { key: 'column5', name: 'Created', minWidth: 100, maxWidth: 100, isResizable: false },
   ];

   const onRenderGroupHeader: IDetailsGroupRenderProps['onRenderHeader'] = (props) => {
      if (props) {

         const group = props.group!;

         return (
            <div className="view-log-link">
               <div className={detailClasses.headerAndFooter} style={{ marginRight: 0, padding: 2, width: 900 }} onClick={() => {

                  //.handler.collapsedIssueGroups.set(group.key, !handler.collapsedIssueGroups.get(group.key));
                  props.onToggleCollapse!(props.group!);
               }}>
                  <div style={{
                     fontSize: "13px",
                     padding: '4px 8px',
                     userSelect: 'none',
                     color: "#404040",
                     width: "100%",
                     fontFamily: "Horde Open Sans SemiBold"
                  }}>{`${group.name} (${group.count})`}</div>
               </div>
            </div>
         );
      }

      return null;
   };


   const renderItem = (item: JobItem, index?: number, column?: IColumn) => {

      if (!column) {
         return <div />;
      }

      if (column.name === "Name") {
         return <Stack horizontal verticalFill={true} verticalAlign="center" tokens={{ childrenGap: 0, padding: 0 }} style={{ whiteSpace: "break-spaces" }} ><Text variant="tiny">{item.job.name}</Text></Stack>;
      }

      if (column.name === "Change") {

         return <Stack horizontal verticalFill={true} verticalAlign="center" tokens={{ childrenGap: 0, padding: 0 }} style={{ paddingTop: 4 }} ><ChangeButton job={item.job} /></Stack>;
      }

      if (column.name === "StartedBy") {
         let startedBy = item.job.startedByUserInfo?.name;
         if (!startedBy) {
            startedBy = "Scheduler";
         }
         return <Stack verticalAlign="center" verticalFill={true} horizontalAlign={"center"}><Text variant="tiny">{startedBy}</Text></Stack>;
      }

      if (column.name === "Step") {
         const ref = jobToStep.get(item.job.id);
         if (!ref || !jobStepName) {
            return null;
         }
         return <Link to={`/job/${ref.jobId}?step=${ref.stepId}`}><Stack horizontal verticalAlign="center" tokens={{ childrenGap: 0, padding: 0 }} style={{ width: "100%", height: "100%" }} >{<StepRefStatusIcon stepRef={ref} />}<Stack style={{ whiteSpace: "break-spaces" }}><Text variant="tiny">{jobStepName}</Text></Stack></Stack></Link>;
      }


      if (column.name === "Created") {

         if (item.job.createTime) {

            const displayTime = moment(item.job.createTime).tz(displayTimeZone());
            const format = dashboard.display24HourClock ? "HH:mm:ss z" : "LT z";

            let displayTimeStr = displayTime.format('MMM Do') + ` at ${displayTime.format(format)}`;


            return <Stack verticalAlign="center" horizontalAlign="end" tokens={{ childrenGap: 0, padding: 0 }} style={{ height: "100%", paddingRight: 18 }}>
               <Stack ><Text variant="tiny">{displayTimeStr}</Text></Stack>
            </Stack>;

         } else {
            return "???";
         }
      }

      return <Stack />;
   }

   const queryJobs = async () => {

      setSearchState({ ...searchState, querying: true });

      let jobs: JobData[] = [];

      try {

         let name = searchState.name?.trim();
         if (!name) {
            name = undefined;
         } else {
            name = "$" + name;
         }

         let minCL: number | undefined = parseInt(searchState.minCL ?? "");
         if (isNaN(minCL)) {
            minCL = undefined;
         }
         let maxCL: number | undefined = parseInt(searchState.maxCL ?? "");
         if (isNaN(maxCL)) {
            maxCL = undefined;
         }

         if (minCL && !maxCL) {
            maxCL = minCL;
         }

         let minCreateTime = searchState.minDate;
         let maxCreateTime = searchState.maxDate;

         if (minCreateTime && (!maxCreateTime || minCreateTime === maxCreateTime)) {

            const maxTime = moment(minCreateTime);
            maxTime.add(23, "hours");
            maxTime.add(59, "minutes");
            maxTime.add(59, "seconds");

            maxCreateTime = maxTime.toDate();

         } else if (maxCreateTime) {

            const maxTime = moment(maxCreateTime);
            maxTime.add(23, "hours");
            maxTime.add(59, "minutes");
            maxTime.add(59, "seconds");

            maxCreateTime = maxTime.toDate();

         }

         const maxCount = 2048;

         let searchStreamId: string | undefined = streamId;

         if (searchState.streamIdOverride) {
            searchStreamId = searchState.streamIdOverride
         } else if (searchState.global && !searchState.containsStep) {
            searchStreamId = undefined;
         }

         const stepName = searchState.containsStep?.trim();
         const stepJobIds: string[] = []

         jobToStep.clear();
         jobStepName = stepName ?? "";
         if (stepName) {

            let stepTemplates: string[] = [];

            if (template?.id) {
               stepTemplates.push(template.id);
            } else {
               stepTemplates = templates.filter(t => !!t.id).map(t => t.id);
            }

            const numTemplates = stepTemplates.length;
            while (stepTemplates.length) {

               const batch = stepTemplates.slice(0, 5);
               stepTemplates = stepTemplates.slice(5);

               // @todo: get endpoint to handle multiple templates
               const results = await Promise.all(batch.map(template => backend.getJobStepHistory(searchStreamId!, stepName, numTemplates === 1 ? 256 : 64, template)));
               results.forEach(r => {
                  r.forEach(j => {
                     stepJobIds.push(j.jobId)
                     jobToStep.set(j.jobId, j);
                  });
               });

               if (stepJobIds.length >= 256) {
                  break;
               }
            }
         }

         const jobIds = stepJobIds.slice(0, 256);

         if (stepName && !jobIds.length) {
            jobs = [];
         } else {

            const query: JobQuery = {
               id: jobIds.length ? jobIds : undefined,
               streamId: searchStreamId?.toLowerCase(),
               name: name,
               filter: "id,name,change,createTime,streamId,preflightChange,startedByUserInfo,streamId,arguments",
               minChange: minCL,
               maxChange: maxCL,
               template: (!searchState.global && template?.id && !searchState.containsStep) ? [template.id] : undefined,
               minCreateTime: minCreateTime?.toUTCString(),
               maxCreateTime: maxCreateTime?.toUTCString(),
               includePreflight: searchState.preflights || !!searchState.preflightOnly,
               preflightOnly: !!searchState.preflightOnly,
               startedByUserId: searchState.userId,
               count: maxCount
            }

            jobs = await backend.getJobs(query, false);

            const p = searchState.containsParameter?.trim().toLowerCase();
            if (p) {
               jobs = jobs.filter(j => {
                  return !!j.arguments.find(a => {
                     return a.toLowerCase().indexOf(p) !== -1
                  })
               });
            }
         }

      } catch (reason) {

      } finally {

         /*
         if (column.name === "Stream") {
             const stream = projectStore.streamById(item.job.streamId);
             if (!stream) {
                 return null;
             }
             return <Stack horizontal verticalFill={true} verticalAlign="center" tokens={{ childrenGap: 0, padding: 0 }} style={{ overflow: "hidden" }} ><Text>{stream.fullname}</Text></Stack>;
         }
         */

         const streamIds = new Set<string>();
         const idToFullname = new Map<string, string>();

         jobs.forEach(j => {
            const stream = projectStore.streamById(j.streamId);
            if (!stream || !stream.fullname) {
               streamIds.add(j.streamId);
               idToFullname.set(j.streamId, j.streamId);
               return;
            };
            streamIds.add(j.streamId);
            idToFullname.set(j.streamId, stream.fullname);

         });

         const sorted = Array.from(streamIds).sort((a, b) => {
            const aname = idToFullname.get(a)!;
            const bname = idToFullname.get(b)!;

            if (aname < bname) return -1;
            if (aname > bname) return 1;
            return 0;
         })

         // emit groups
         const groups: IGroup[] = [];
         const items: JobItem[] = [];
         sorted.forEach(id => {
            const streamJobs = jobs.filter(j => j.streamId === id);
            groups.push({ key: `jobs_${id}`, name: idToFullname.get(id)!, startIndex: items.length, count: streamJobs.length, isCollapsed: false });
            items.push(...streamJobs.map(j => { return { job: j } }));
         })


         setSearchState({ ...searchState, items: items, groups: groups, querying: false });
      }

   }

   const renderRow: IDetailsListProps['onRenderRow'] = (props) => {

      if (props) {

         const item = props!.item as JobItem;

         const url = `/job/${item.job.id}`;

         const commonSelectors = { ".ms-DetailsRow-cell": { "overflow": "visible", padding: 0 } };

         props.styles = { ...props.styles, root: { selectors: { ...commonSelectors as any } } };

         return <Link to={url} onClick={(ev) => { if (!ev.ctrlKey) onClose() }}><div className="job-item"><DetailsRow {...props} /> </div></Link>;

      }
      return null;
   };

   let title = `Find Jobs in ${stream.fullname}`;
   if (searchState.global) {
      title = `Find Jobs in All Streams`;
   }
   if (searchState.streamIdOverride) {
      title = `Find Jobs in ${searchState.streamIdOverride}`;
   }

   return (<Modal isOpen={true} topOffsetFixed={true} styles={{ main: { padding: 8, width: 1400, height: '800px', backgroundColor: hordeTheme.horde.contentBackground, hasBeenOpened: false, top: "80px", position: "absolute" } }} className={hordeClasses.modal} onDismiss={() => { onClose() }}>
      {searchState.querying && <StatusModal text={"Finding Jobs"} />}
      <Stack styles={{ root: { paddingTop: 8, paddingLeft: 24, paddingRight: 12, paddingBottom: 8 } }}>
         <Stack tokens={{ childrenGap: 12 }}>
            <Stack horizontal styles={{ root: { padding: 0 } }}>
               <Stack style={{ paddingLeft: 0, paddingTop: 4 }} grow>
                  <Text variant="mediumPlus" styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>{title}</Text>
               </Stack>
               <Stack grow horizontalAlign="end">
                  <IconButton
                     iconProps={{ iconName: 'Cancel' }}
                     onClick={() => { onClose(); }}
                  />
               </Stack>
            </Stack>

            <Stack horizontal tokens={{ childrenGap: 48 }} >

               <Stack>
                  <Stack tokens={{ childrenGap: 12 }}>

                     <Stack>
                        <Label>Template</Label>
                        <DefaultButton disabled={searchState.global || !!searchState.streamIdOverride} style={{ width: 352, textAlign: "left" }} menuProps={templateMenuProps} text={template ? template.name : "All Templates"} />
                     </Stack>

                     <Stack>
                        <TextField value={searchState.name ?? ""} spellCheck={false} autoComplete="off" style={{ width: 350 }} label="Name" onChange={(event, newValue) => {
                           setSearchState({ ...searchState, name: newValue })

                        }} ></TextField>
                     </Stack>


                     <Stack horizontal tokens={{ childrenGap: 12 }} >
                        <TextField value={searchState.minCL ?? ""} style={{ width: 168 }} label="Min Changelist" onChange={(ev, newValue) => {
                           newValue = newValue?.trim();
                           if (!newValue) {
                              newValue = undefined;
                           }
                           setSearchState({ ...searchState, minCL: newValue })
                        }} />
                        <TextField value={searchState.maxCL ?? ""} style={{ width: 168 }} label="Max Changelist" onChange={(ev, newValue) => {
                           newValue = newValue?.trim();
                           if (!newValue) {
                              newValue = undefined;
                           }
                           setSearchState({ ...searchState, maxCL: newValue })
                        }} />
                     </Stack>


                     <Stack horizontal tokens={{ childrenGap: 12 }} >
                        <DatePicker value={searchState.minDate} key="setMinDate" label="Min Date" onSelectDate={(date) => { setSearchState({ ...searchState, minDate: date ? date : undefined }) }} style={{ width: 168 }} />
                        <DatePicker value={searchState.maxDate} label="Max Date" onSelectDate={(date) => { setSearchState({ ...searchState, maxDate: date ? date : undefined }) }} style={{ width: 168 }} />
                     </Stack>

                     <Stack>
                        <TextField value={searchState.streamIdOverride ?? ""} spellCheck={false} autoComplete="off" style={{ width: 350 }} label="Stream Id" onChange={(event, newValue) => {
                           const templateId = searchState.templateId;
                           const global = searchState.global;
                           setSearchState({ ...searchState, streamIdOverride: newValue, global: newValue ? false : global, templateId: newValue ? undefined : templateId })

                        }} ></TextField>
                     </Stack>

                     <Stack>
                        <TextField disabled={!template} value={searchState.containsParameter ?? ""} spellCheck={false} autoComplete="off" style={{ width: 350 }} label="Contains Parameter" placeholder={!template ? "Please select a template to search parameters" : ""} onChange={(event, newValue) => {
                           setSearchState({ ...searchState, containsParameter: newValue?.trim() ? newValue : undefined })
                        }} ></TextField>
                     </Stack>

                     <Stack>
                        <TextField value={searchState.containsStep ?? ""} spellCheck={false} autoComplete="off" style={{ width: 350 }} label="Contains Step" description="Note: Exact match is required" onChange={(event, newValue) => {
                           setSearchState({ ...searchState, containsStep: newValue?.trim() ? newValue : undefined })
                        }} ></TextField>
                     </Stack>

                     <Stack tokens={{ childrenGap: 4 }}>
                        <Label>Started By</Label>
                        <UserSelect handleSelection={(id => {
                           setSearchState({ ...searchState, userId: id })
                        })} userIdHints={[]} noResultsFoundText="No users found" />
                     </Stack>

                     <Stack horizontal tokens={{ childrenGap: 18 }}>
                        <Stack verticalFill={true} verticalAlign="center">
                           <Checkbox checked={searchState.preflights} label="Include Preflights" onChange={(ev, checked) => {
                              setSearchState({ ...searchState, preflights: checked ? checked : false })
                           }} />
                        </Stack>

                        <Stack verticalFill={true} verticalAlign="center">
                           <Checkbox checked={searchState.preflightOnly ?? false} label="Only Preflights" onChange={(ev, checked) => {
                              setSearchState({ ...searchState, preflightOnly: checked ? checked : undefined })
                           }} />
                        </Stack>
                     </Stack>


                     <Stack verticalFill={true} verticalAlign="center">
                        <Checkbox checked={searchState.global} label="Global Search" onChange={(ev, checked) => {
                           const templateId = searchState.templateId;
                           const streamIdOverride = searchState.streamIdOverride;
                           setSearchState({ ...searchState, global: checked ? checked : false, templateId: checked ? undefined : templateId, streamIdOverride: checked ? undefined : streamIdOverride })
                        }} />
                     </Stack>

                     <Stack horizontal style={{ paddingTop: 24 }}>
                        <Stack style={{ paddingTop: 8, paddingRight: 24 }}>
                           <PrimaryButton disabled={searchState.querying} text="Search" onClick={() => { queryJobs() }} />
                        </Stack>
                        <Stack style={{ paddingTop: 8, paddingRight: 24 }}>
                           <DefaultButton disabled={searchState.querying} text="Clear" onClick={() => { setSearchState({ items: [], groups: [], querying: false, preflights: false, userId: searchState.userId }) }} />
                        </Stack>
                     </Stack>
                  </Stack>
               </Stack>

               <Stack>
                  <Stack styles={{ root: { paddingLeft: 4, paddingRight: 0, paddingBottom: 4 } }}>
                     <Stack>
                        <Label>Jobs</Label>
                     </Stack>

                     {!searchState.items.length && <Stack>
                        <Text>No Results</Text>
                     </Stack>}

                     {!!searchState.items.length && <Stack>
                        <div style={{ overflowY: 'auto', overflowX: 'hidden', height: "670px" }} data-is-scrollable={true}>
                           <Stack tokens={{ childrenGap: 12 }} style={{ paddingRight: 12 }}>
                              <DetailsList
                                 compact={true}
                                 isHeaderVisible={false}
                                 indentWidth={0}
                                 items={searchState.items}
                                 groups={searchState.groups}
                                 columns={columns}
                                 setKey="set"
                                 selectionMode={SelectionMode.none}
                                 onRenderItemColumn={renderItem}
                                 onRenderRow={renderRow}
                                 groupProps={{
                                    onRenderHeader: onRenderGroupHeader,
                                 }}

                              />
                           </Stack>
                        </div>
                     </Stack>}
                  </Stack>
               </Stack>

            </Stack>
         </Stack>
      </Stack>
   </Modal>);
});


