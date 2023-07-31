// Copyright Epic Games, Inc. All Rights Reserved.

import { Checkbox, ComboBox, DefaultButton, DetailsList, DetailsListLayoutMode, Dialog, DialogFooter, DialogType, IColumn, IComboBoxOption, MessageBar, MessageBarType, Position, PrimaryButton, ScrollablePane, ScrollbarVisibility, SelectionMode, Slider, SpinButton, Spinner, SpinnerSize, Stack, Text, TextField } from "@fluentui/react";
import { observer } from "mobx-react-lite";
import React, { useEffect, useState } from "react";
import { Link, useParams } from "react-router-dom";
import backend from "../backend";
import { GetAgentResponse, GetBatchResponse, GetJobTimingResponse, GetPoolResponse, GetStepResponse, JobData, JobQuery, JobState, JobStepBatchState, JobStepState, PoolSizeStrategy, StepData, UpdatePoolRequest } from "../backend/Api";
import dashboard from "../backend/Dashboard";
import { PollBase } from "../backend/PollBase";
import { projectStore } from "../backend/ProjectStore";
import { useWindowSize } from "../base/utilities/hooks";
import { getNiceTime, getStepElapsed, getStepETA, getStepFinishTime, getStepStartTime } from "../base/utilities/timeUtils";
import { hordeClasses, linearInterpolate, modeColors } from "../styles/Styles";
import { Breadcrumbs } from "./Breadcrumbs";
import { HistoryModal } from "./HistoryModal";
import { StepStatusIcon } from "./StatusIcon";
import { TopNav } from "./TopNav";


type PendingBatch = {
   job: JobData;
   batch: GetBatchResponse;
}

class PoolHandler extends PollBase {

   constructor(pollTime = 15000) {

      super(pollTime);

   }

   clear() {
      this.poolId = undefined;
      this.pool = undefined;
      this.agents = undefined;
      this.agentJobs = new Map();
      this.activeSteps = new Map();
      this.pendingSteps = new Map();
      this.pendingBatches = [];
      this.jobData = new Map();
      super.stop();
   }

   set(poolId?: string) {

      if (poolId === this.poolId) {
         return;
      }

      if (poolId) {
         this.clear();
         this.poolId = poolId;
         this.start();
      } else {
         this.poolId = undefined;
      }

   }

   async poll(): Promise<void> {

      if (!this.poolId) {
         return;
      }

      try {

         const doTimings = !!this.pool;

         const pool = this.pool = await backend.getPool(this.poolId);

         // @todo: optimize using modifiedAfter
         const agents = this.agents = await backend.getAgents({ poolId: this.poolId });

         this.agentJobs = new Map();
         this.activeSteps = new Map();
         this.pendingSteps = new Map();
         this.pendingBatches = [];
         this.jobData = new Map();

         const streamJobIds = new Set<string>();

         const wstreams = new Set<string>();

         projectStore.projects.map(p => p.streams ?? []).flat().filter(s => {
            for (let v in s.agentTypes) {
               if (s.agentTypes[v].pool === pool.id) {
                  return true;
               }
            }
            return false;

         }).forEach(s => wstreams.add(s.id));


         // build up stream job list
         let poolStreams = Array.from(wstreams);

         const minCreateTime = new Date(Math.round((new Date().getTime() / 1000) - (48 * 3600)) * 1000).toISOString();

         while (poolStreams.length) {

            const batch = poolStreams.slice(0, 5);

            const requests = batch.map(streamId => {

               const query: JobQuery = {
                  streamId: streamId,
                  filter: "id,state",
                  minCreateTime: minCreateTime,
                  count: 250
               }
               return backend.getJobs(query, false);

            }).filter(r => !!r);

            await Promise.all(requests).then((responses) => {

               responses.forEach(r => {

                  r!.forEach(j => {
                     if (j.state !== JobState.Complete) {
                        streamJobIds.add(j.id);
                     }
                  });
               });

            }).catch((errors) => {
               console.log(errors);
               // eslint-disable-next-line
            }).finally(() => {
               poolStreams = poolStreams.slice(5);
            });
         }

         if (streamJobIds.size) {

            const streamJobs = await backend.getJobs({ id: Array.from(streamJobIds), filter: "id,name,batches,change,createTime,streamId,graphHash" }, true);

            streamJobs.forEach(job => {

               job.batches?.forEach(b => {

                  if (b.agentId) {
                     return;
                  }

                  if (!b.steps.length || (b.steps[0].state === JobStepState.Aborted || b.steps[0].state === JobStepState.Skipped)) {
                     return;
                  }

                  const groups = job.graphRef?.groups;

                  if (!groups || !groups[b.groupIdx]) {
                     return;
                  }

                  const group = groups[b.groupIdx];

                  const agentType = group?.agentType;

                  const stream = projectStore.streamById(job.streamId);

                  if (!stream) {
                     return;
                  }

                  const agentPool = stream.agentTypes[agentType!]?.pool;

                  if (!agentPool || agentPool !== this.pool?.id) {
                     return;
                  }

                  this.pendingBatches.push({ job: job, batch: b });

               })
            })
         }

         const jobIds = agents.map(agent => {

            const lease = agent.leases?.find(lease => {
               return lease.details && lease.details["jobId"];
            });

            return {
               agent: agent,
               jobId: lease?.details!["jobId"]
            }

         }).filter(jobInfo => !!jobInfo.jobId).map(jobInfo => {
            this.agentJobs.set(jobInfo.agent.id, jobInfo.jobId!);
            return jobInfo.jobId!
         });


         const activeJobs: JobData[] = [];

         if (jobIds.length) {

            const query: JobQuery = {
               id: jobIds,
               filter: "id,name,batches,change,createTime,streamId,graphHash",
            }

            const jobs = await backend.getJobs(query, true);

            jobs.forEach(job => {

               this.jobData.set(job.id, job);

               job.batches?.forEach(batch => {

                  if (!this.agents!.find(a => a.id === batch.agentId) || !batch.startTime || batch.finishTime) {
                     return;
                  }

                  if (!batch) {
                     return;
                  }

                  const active = batch.steps?.find(step => !!step.startTime && !step.finishTime);
                  const pending = !!batch?.startTime && batch.steps?.find(step => !step.startTime && step.state !== JobStepState.Skipped && step.state !== JobStepState.Aborted);

                  if (active) {
                     if (!activeJobs.find(j => j.id === job.id)) {
                        activeJobs.push(job);
                     }
                     this.activeSteps.set(batch.agentId!, active);
                  }
                  if (pending) {
                     this.pendingSteps.set(batch.agentId!, pending);
                  }

               });

            });
         }

         if (doTimings) {

            let jobTiming = activeJobs.filter(job => !this.jobTiming.get(job.id));

            while (jobTiming.length) {

               const batch = jobTiming.slice(0, 5);

               const requests = batch.map(job => backend.getJobTiming(job.id));

               await Promise.all(requests).then((responses) => {

                  responses.forEach(r => {
                     this.jobTiming.set(r.jobResponse.id, r);
                  });

               }).catch((errors) => {
                  console.log(errors);
                  // eslint-disable-next-line
               }).finally(() => {

                  jobTiming = jobTiming.slice(5);
               });
            }

         }

         activeJobs.forEach((job) => {
            const timing = this.jobTiming.get(job.id);
            if (timing) {
               const steps = (job.batches?.map(b => b.steps).flat() ?? []) as StepData[];
               steps.forEach(step => step.timing = timing.steps[step.id] ? timing.steps[step.id] : undefined)
            }
         });

         this.setUpdated();

      } catch (err) {

      }

   }

   poolId?: string;
   // agentId => jobId
   agentJobs: Map<string, string> = new Map();

   // agentId => step
   activeSteps: Map<string, GetStepResponse> = new Map();

   // agentId => step
   pendingSteps: Map<string, GetStepResponse> = new Map();

   // list of batches which don't have an agent
   pendingBatches: PendingBatch[] = [];

   jobData: Map<string, JobData> = new Map();
   jobTiming: Map<string, GetJobTimingResponse> = new Map();
   agents?: GetAgentResponse[];
   pool?: GetPoolResponse;

}

const handler = new PoolHandler();

enum StepState {
   Active = "Active",
   Pending = "Pending",
   Previous = "Previous"
}

const StepPanel: React.FC<{ stepState: StepState }> = ({ stepState }) => {

   const [lastSelectedAgent, setLastSelectedAgent] = useState<string | undefined>(undefined);

   const pool = handler.pool;
   if (!pool || !handler.agents || !handler.agents.length) {
      return null;
   }

   let agents = [...handler.agents];

   // active steps
   if (stepState === StepState.Active) {

      agents = agents.filter(a => !!handler.activeSteps.get(a.id));

      agents = agents.sort((a, b) => {
         const stepA = handler.activeSteps.get(a.id)!;
         const stepB = handler.activeSteps.get(b.id)!;
         return new Date(stepA.startTime!) < new Date(stepB.startTime!) ? -1 : 1;

      });

   }

   // pending steps
   if (stepState === StepState.Pending) {

      agents = agents.filter(a => !!handler.pendingSteps.get(a.id));

      agents = agents.sort((a, b) => {
         const jobA = handler.jobData.get(handler.agentJobs.get(a.id)!)!;
         const jobB = handler.jobData.get(handler.agentJobs.get(b.id)!)!;
         const stepA = handler.pendingSteps.get(a.id)!;
         const stepB = handler.pendingSteps.get(b.id)!;
         const batchA = jobA?.batches?.find(b => b.steps.find(s => s.id === stepA?.id))!;
         const batchB = jobB?.batches?.find(b => b.steps.find(s => s.id === stepB?.id))!;

         if (!batchA?.startTime || !batchB?.startTime) {
            console.error("Bad batch start time in pool view");
            return 0;
         }
         return new Date(batchA.startTime!) < new Date(batchB.startTime!) ? -1 : 1;

      });

   }


   type AgentItem = {
      agent: GetAgentResponse;
   }

   let columns: IColumn[] = [];

   columns = [
      { key: 'column1', name: 'Job', minWidth: 600, maxWidth: 600 },
      { key: 'column2', name: 'Agent', minWidth: 100, maxWidth: 100 },
      { key: 'column3', name: 'Time Active', minWidth: 100, maxWidth: 100 },
      { key: 'column4', name: 'Start Time', minWidth: 100, maxWidth: 100 },
      { key: 'column5', name: 'Estimated Finish', minWidth: 100, maxWidth: 100 },
      { key: 'column6', name: 'View Log', minWidth: 80, maxWidth: 80 }
   ];

   const agentItems: AgentItem[] = agents.map(a => {
      return {
         agent: a
      }
   }) ?? [];

   const onRenderItemColumn = (item: AgentItem, index?: number, columnIn?: IColumn) => {

      const column = columnIn!;
      const agent = item.agent;
      const job = handler.jobData.get(handler.agentJobs.get(agent.id)!);
      let step: GetStepResponse | undefined;
      if (stepState === StepState.Active) {
         step = handler.activeSteps.get(agent.id);
      }
      if (stepState === StepState.Pending) {
         step = handler.pendingSteps.get(agent.id);
      }

      if (!job || !job.graphRef?.groups || !step) {
         return null;
      }

      const batch = job.batches!.find(b => b.steps.find(s => s.id === step!.id));
      if (!batch) {
         return null;
      }
      const group = job.graphRef.groups[batch.groupIdx];
      const node = group.nodes[step.nodeIdx];

      if (column.name === "Time Active") {
         return <Stack horizontalAlign={"end"}><Text>{getStepElapsed(step)}</Text></Stack>;
      }

      if (column.name === "Start Time") {
         const started = getStepStartTime(step);
         if (!started.display || !started.server) {
            return null;
         } else {
            return <Stack horizontalAlign={"end"}><Text style={{ fontSize: "13px" }}>{started.display}</Text></Stack>;
         }
      }

      if (column.name === 'Estimated Finish') {

         let eta = {
            display: "",
            server: ""
         };

         let finished = { display: "", server: "" };

         eta = getStepETA(step, job);

         finished = getStepFinishTime(step);

         if (finished.display) {
            eta.display = finished.display;
            eta.server = finished.server;
         }

         let time = eta.display;

         const etaColor = dashboard.darktheme ? "#A9A9A9" : "#999999";

         const color = !step.finishTime ? etaColor : undefined;

         // Open Sans tilde rendering issue at 13px, and not rendering at all at other px: https://github.com/google/fonts/issues/399, do not change from 12px
         return <Stack horizontalAlign={"end"}>
            <Stack horizontal tokens={{ childrenGap: 2 }}>
               {!!time && !step.finishTime && <Text style={{ color: color, fontSize: "11px", paddingTop: 2 }}>~</Text>}
               <Text style={{ color: color, fontSize: "13px" }}>
                  {time}
               </Text>
            </Stack>
         </Stack>;

      }

      if (column.name === "View Log") {
         if (stepState === StepState.Pending) {
            return null;
         }
         return <Stack style={{ paddingRight: 32 }}>
            <Link to={`/log/${step.logId}`} target="_blank">
               <Stack horizontal horizontalAlign={"end"} verticalAlign="center" tokens={{ childrenGap: 0, padding: 0 }} style={{ width: "100%", height: "100%" }}>
                  <Text styles={{ root: { margin: '0px', padding: '0px', paddingRight: '8px' } }} className={"view-log-link"}>View Log</Text>
               </Stack>
            </Link>
         </Stack>
      }


      if (column.name === "Job") {
         let jobName = job.name;
         if (jobName.indexOf("- Kicked By") !== -1) {
            jobName = jobName.split("- Kicked By")[0];
         }
         const stepName = `${jobName} - ${node.name}`;
         const stepUrl = `/job/${job.id}?step=${step.id}`;

         return <Stack horizontal>
            <StepStatusIcon step={step} />
            <Link target="_blank" to={stepUrl}><Text>{stepName}</Text></Link>
         </Stack>;

      }

      if (column.name === "Agent") {
         return <div style={{ cursor: "pointer" }} onClick={() => { setLastSelectedAgent(agent.id) }}><Text>{agent.name}</Text></div>

      }

      return null;
   }

   const count = Math.min(agentItems.length, stepState === StepState.Active ? 8 : 6);
   let height = count * 32;

   return (<Stack>
      <HistoryModal agentId={lastSelectedAgent} onDismiss={() => setLastSelectedAgent(undefined)} />
      <Stack styles={{ root: { paddingTop: 18, paddingLeft: 12, paddingRight: 12 } }} >
         <Stack tokens={{ childrenGap: 12 }}>
            <Stack>
               <Text variant="mediumPlus" styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>{stepState} Steps ({agentItems.length})</Text>
            </Stack>
            <Stack style={{ position: "relative", height: height, width: 1280 }}>
               <ScrollablePane scrollbarVisibility={ScrollbarVisibility.always}>
                  <DetailsList
                     styles={{
                        root: {
                           selectors: {
                              '.ms-List-cell': {
                                 color: modeColors.text,
                                 minHeight: 22,
                                 fontSize: 12
                              }
                           }
                        }
                     }}
                     compact={true}
                     items={agentItems}
                     columns={columns}
                     setKey="set"
                     layoutMode={DetailsListLayoutMode.justified}
                     isHeaderVisible={false}
                     selectionMode={SelectionMode.none}
                     onRenderItemColumn={onRenderItemColumn}
                  />
               </ScrollablePane>
            </Stack>
         </Stack>
      </Stack>
   </Stack>)
};


const BatchPanel: React.FC = () => {

   const pool = handler.pool;
   if (!pool || !handler.agents || !handler.agents.length) {
      return null;
   }

   let batches = handler.pendingBatches.sort((a, b) => {

      if (a.batch.state === JobStepBatchState.Ready && b.batch.state === JobStepBatchState.Waiting) {
         return -1;
      }

      if (b.batch.state === JobStepBatchState.Ready && a.batch.state === JobStepBatchState.Waiting) {
         return 1;
      }

      return new Date(a.job.createTime) < new Date(b.job.createTime) ? -1 : 1;

   });

   type BatchItem = {
      pending: PendingBatch;
   }

   let columns: IColumn[] = [];

   columns = [
      { key: 'column1', name: 'Job', minWidth: 600, maxWidth: 600 },
      { key: 'column4', name: 'Job Created', minWidth: 300, maxWidth: 300 },
      { key: 'column2', name: 'Status', minWidth: 200, maxWidth: 200 },
   ];

   const batchItems: BatchItem[] = batches.map(b => {
      return {
         pending: b
      }
   }) ?? [];

   const onRenderItemColumn = (item: BatchItem, index?: number, columnIn?: IColumn) => {

      const column = columnIn!;
      const job = item.pending.job;
      const batch = item.pending.batch;

      if (column.name === "Status") {
         let statusText = "";

         if (batch.state === JobStepBatchState.Waiting) {
            statusText = "Waiting for dependencies";
         }

         if (batch.state === JobStepBatchState.Ready) {
            statusText = `Waiting for agent`;
         }

         return <Text>{statusText}</Text>
      }

      if (column.name === "Job Created") {

         return <Stack ><Text style={{ fontSize: "13px", paddingRight: 12 }}>{getNiceTime(job.createTime, false)}</Text></Stack>;
      }


      if (column.name === "Job") {
         let jobName = job.name;
         if (jobName.indexOf("- Kicked By") !== -1) {
            jobName = jobName.split("- Kicked By")[0];
         }
         let step: GetStepResponse | undefined;
         if (batch.steps.length) {
            step = batch.steps[0];
            const group = job.graphRef!.groups![batch.groupIdx];
            const node = group.nodes[step.nodeIdx];
            jobName += ` - ${node.name}`;
         }

         const jobUrl = `/job/${job.id}`;

         return <Stack horizontal>
            {!!step && <StepStatusIcon step={step} />}
            <Link target="_blank" to={jobUrl}><Text>{jobName}</Text></Link>
         </Stack>;

      }

      return null;
   }

   const count = Math.min(batchItems.length, 8);
   let height = 60 + count * 32;

   return (<Stack>
      <Stack styles={{ root: { paddingTop: 18, paddingLeft: 12, paddingRight: 12 } }} >
         <Stack tokens={{ childrenGap: 12 }}>
            <Stack>
               <Text variant="mediumPlus" styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>Pending Batches ({batchItems.length})</Text>
            </Stack>
            {!!batchItems.length && <Stack style={{ position: "relative", height: height, width: 1280 }}>
               <ScrollablePane scrollbarVisibility={ScrollbarVisibility.always}>
                  <DetailsList
                     styles={{
                        root: {
                           selectors: {
                              '.ms-List-cell': {
                                 color: modeColors.text,
                                 minHeight: 22,
                                 fontSize: 12
                              }
                           }
                        }
                     }}
                     compact={true}
                     items={batchItems}
                     columns={columns}
                     setKey="set"
                     layoutMode={DetailsListLayoutMode.justified}
                     isHeaderVisible={true}
                     selectionMode={SelectionMode.none}
                     onRenderItemColumn={onRenderItemColumn}
                  />
               </ScrollablePane>
            </Stack>}
         </Stack>
      </Stack>
   </Stack>)
};

const AutoScalerPanel: React.FC = () => {

   const [state, setState] = useState<{ error?: string, submitting?: boolean, confirmed?: boolean, modified?: boolean, name?: string, color?: string, autoscale?: boolean, minAgents?: number, reserveAgents?: number, strategy?: PoolSizeStrategy, conformInterval?: number }>({});

   const pool = handler.pool;
   if (!pool) {
      return null;
   }

   const stratItems: IComboBoxOption[] = [
      { key: PoolSizeStrategy.LeaseUtilization, text: 'Lease Utilization', data: PoolSizeStrategy.LeaseUtilization },
      { key: PoolSizeStrategy.JobQueue, text: 'Job Queue', data: PoolSizeStrategy.JobQueue }
   ];

   if (state.autoscale === undefined) {

      let color = "1";
      if (pool.properties && pool.properties["Color"]) {
         color = pool.properties["Color"];
      }

      setState({ name: pool.name, color: color, autoscale: pool.enableAutoscaling ?? false, minAgents: pool.minAgents ?? 0, reserveAgents: pool.numReserveAgents ?? 0, strategy: pool.sizeStrategy ?? PoolSizeStrategy.LeaseUtilization, conformInterval: pool.conformInterval ?? 24 });
      return null;
   }

   let submitDialog: JSX.Element | undefined;

   if (state.submitting) {

      const poolName = state.name?.trim();

      if (!poolName) {
         setState({ ...state, submitting: false, confirmed: false, error: "Please specify a pool name" });
         return null;
      }
      if (!state.color) {
         setState({ ...state, submitting: false, confirmed: false, error: "Invalid color setting, this is a bug" });
         return null;
      }

      if (!state.strategy) {
         setState({ ...state, submitting: false, confirmed: false, error: "Invalid strategy setting, this is a bug" });
         return null;
      }

      if (!state.confirmed) {

         const onSubmit = async () => {

            try {

               const update: UpdatePoolRequest = {
                  name: poolName,
                  enableAutoscaling: state.autoscale ?? false,
                  minAgents: state.minAgents ?? 0,
                  numReserveAgents: state.reserveAgents ?? 0,
                  sizeStrategy: state.strategy!,
                  conformInterval: typeof (state.conformInterval) === "number" ? state.conformInterval : 24,
                  properties: { Color: state.color! }
               }

               await backend.updatePool(pool.id, update);

               handler.pool = await backend.getPool(pool.id);

               setState({ ...state, submitting: false, confirmed: undefined, modified: false, error: undefined });

            } catch (reason) {
               setState({ ...state, submitting: false, confirmed: false, error: (reason as any).toSrtring() })
            }
         }

         const close = () => { setState({ ...state, submitting: undefined }) };

         submitDialog = <Dialog
            onDismiss={() => close()}
            isOpen={true}
            minWidth={600}
            dialogContentProps={{
               type: DialogType.largeHeader,
               title: `Confirm Settings for ${pool.name}`,
               isMultiline: true
            }}
            modalProps={{
               isBlocking: true,
               //styles: { main: { minHeight: 600 } },
            }}>
            <Stack tokens={{ childrenGap: 12 }}>
               <Text>{`Pool Name: ${poolName}`}</Text>
               <Text>{`Conform Interval: ${state.conformInterval ?? "24"}`}</Text>
               <Text>{`Autoscale: ${state.autoscale ? "On" : "Off"}`}</Text>
               <Text>{`Minimum Agents: ${state.minAgents ?? 0}`}</Text>
               <Text>{`Reserve Agents: ${state.reserveAgents ?? 0}`}</Text>
               <Text>{`Pool Strategy: ${state.strategy}`}</Text>

            </Stack>
            <DialogFooter>
               <PrimaryButton onClick={() => { onSubmit(); setState({ ...state, confirmed: true }) }} text={state.error ? "Try Again" : "Confirm"} />
               <DefaultButton onClick={() => { close() }} text="Cancel" />
            </DialogFooter>
         </Dialog>
      }
   }

   const disabled = !dashboard.hordeAdmin || state.submitting;

   let selectedColor = state.color ?? "0";
   let defaultColor = "0";

   if (pool.properties && pool.properties["Color"]) {
      defaultColor = pool.properties["Color"];
   }

   return <Stack>
      {!!submitDialog && submitDialog}
      <Stack style={{ minWidth: 320 }} tokens={{ childrenGap: 12 }}>
         <Stack>
            <Text variant="mediumPlus" styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>Settings</Text>
            {!!state.error && <MessageBar
               messageBarType={MessageBarType.error}
               isMultiline={false}> {`Error submitting changes: ${state.error}`} </MessageBar>}
         </Stack>
         <Stack horizontal tokens={{ childrenGap: 32 }}>
            <Stack tokens={{ childrenGap: 8 }}>
               <Stack>
                  <TextField label="Pool Name" value={state.name} style={{ width: 240 }} onChange={(ev, newValue) => {
                     ev.preventDefault();
                     setState({ ...state, name: newValue ? newValue : "", modified: true })
                  }} />
               </Stack>
               <Stack horizontal style={{ width: 240 }}>
                  <Stack grow>
                     <Slider
                        styles={{ root: { paddingTop: 15 } }}
                        label="Color"
                        onChange={(value: number) => setState({ ...state, modified: true, color: value.toString() })}
                        defaultValue={parseInt(defaultColor)}
                        max={599}
                        showValue={false}
                     />
                  </Stack>
                  <Stack>
                     <div className={hordeClasses.colorPreview} style={{ backgroundColor: linearInterpolate(selectedColor) }} />
                  </Stack>
               </Stack>

               {dashboard.hordeAdmin && <Stack style={{ paddingTop: 24 }}>
                  <PrimaryButton disabled={disabled || !state.modified} text="Save Changes" onClick={() => { setState({ ...state, submitting: true, confirmed: false, error: undefined }) }} />
               </Stack>}
            </Stack>
            <Stack style={{ paddingLeft: 12 }} tokens={{ childrenGap: 8 }}>
               <Checkbox styles={{ root: { paddingTop: 12 } }} disabled={disabled} label="Autoscaling" checked={state.autoscale} onChange={(ev, checked) => {
                  setState({ ...state, modified: true, autoscale: checked })
               }} />
               <Stack horizontal tokens={{ childrenGap: 18 }}>
                  <ComboBox styles={{ root: { width: 180 } }} disabled={disabled} label="Strategy" selectedKey={state.strategy} options={stratItems} onChange={(ev, option, index, value) => {
                     setState({ ...state, modified: true, strategy: option!.data })
                  }} />
                  <SpinButton styles={{ root: { width: 128 } }} disabled={disabled} label="Minimum Agents" value={state.minAgents?.toString() ?? "0"} labelPosition={Position.top} onChange={(ev, value) => {
                     setState({ ...state, modified: true, minAgents: parseInt(value ?? "0") })
                  }} />
                  <SpinButton styles={{ root: { width: 128 } }} disabled={disabled} label="Reserve Agents" value={state.reserveAgents?.toString() ?? "0"} labelPosition={Position.top} onChange={(ev, value) => {
                     setState({ ...state, modified: true, reserveAgents: parseInt(value ?? "0") })
                  }} />
               </Stack>
               <Stack>
                  <SpinButton styles={{ root: { width: 180 } }} label={`Conform Interval${state.conformInterval === 0 ? " (Disabled)" : " (Hours)"}`} min={0} max={24 * 7} value={state.conformInterval?.toString() ?? "24"} labelPosition={Position.top} onChange={(ev, value) => {
                     setState({ ...state, modified: true, conformInterval: parseInt(value ?? "24") })
                  }} />
               </Stack>
            </Stack>
         </Stack>
      </Stack>
   </Stack>
}


const PoolPanel: React.FC = () => {

   const pool = handler.pool;
   if (!pool) {
      return null;
   }

   type WorkspaceItem = {
      identifier: string;
      stream: string;
      incremental: boolean;
   }

   const columns = [
      { key: 'column1', name: 'Identifier', minWidth: 100, maxWidth: 100 },
      { key: 'column2', name: 'Stream', minWidth: 100, maxWidth: 200 },
      //{ key: 'column3', name: 'Incremental', minWidth: 100, maxWidth: 200 },
   ];

   const workspaceItems: WorkspaceItem[] = pool.workspaces.map(w => {
      return {
         identifier: w.identifier,
         stream: w.stream,
         incremental: w.bIncremental
      }
   });

   const onRenderItemColumn = (item: WorkspaceItem, index?: number, columnIn?: IColumn) => {

      const column = columnIn!;

      // simple cases
      switch (column.name) {
         case 'Identifier':
            return <Text>{item.identifier}</Text>
         case 'Stream':
            return <Text>{item.stream}</Text>
         case 'Incremental':
            return <Text >{item.incremental ? "True" : "False"}</Text>
         default:
            break;
      }

      return null;
   }

   type SummaryItem = {
      name: string;
      value: string;
   }

   const summaryColumns = [
      { key: 'column1', name: 'Name', minWidth: 100, maxWidth: 100 },
      { key: 'column2', name: 'Value', minWidth: 100, maxWidth: 200 },
   ];

   const onRenderSummaryItemColumn = (item: SummaryItem, index?: number, columnIn?: IColumn) => {

      const column = columnIn!;

      if (column.name === "Name") {
         return item.name;
      }

      if (column.name === "Value") {
         return item.value;
      }

      return null;
   }

   const summaryItems: SummaryItem[] = [];
   const agents = handler.agents;
   if (agents?.length) {
      const total = agents.length;
      const active = agents.filter(a => !!a.leases?.length).length;
      const offline = agents.filter(a => !a.online).length;
      const disabled = agents.filter(a => !a.enabled).length;
      const ready = agents.filter(a => !a.leases?.length && a.online && a.enabled && !a.pendingConform && !a.pendingFullConform).length;

      const percent = (value: number) => {
         return value.toLocaleString(undefined, { style: 'percent', minimumFractionDigits: 0 })
      }

      summaryItems.push({ name: "Agents", value: `${total} ` });
      summaryItems.push({ name: "Active", value: `${percent(active / total)} (${active})` });
      summaryItems.push({ name: "Ready", value: `${percent(ready / total)} (${ready})` });
      summaryItems.push({ name: "Offline", value: `${percent(offline / total)} (${offline})` });
      summaryItems.push({ name: "Disabled", value: `${percent(disabled / total)} (${disabled})` });
   }

   let color = "1";
   if (pool.properties && pool.properties["Color"]) {
      color = pool.properties["Color"];
   }


   return (<Stack>
      <Stack styles={{ root: { paddingTop: 18, paddingLeft: 12, paddingRight: 12, width: "100%" } }} >
         <Stack tokens={{ childrenGap: 12 }}>
            <Stack horizontal tokens={{ childrenGap: 118 }}>
               <Stack style={{ minWidth: 224 }}>
                  <Stack style={{ paddingBottom: 8 }}>
                     <PrimaryButton text={pool.name} href={`/agents?search=${encodeURI(pool.id)}`} target="_blank" style={{ color: "#FFFFFF", backgroundColor: linearInterpolate(color), border: "unset", flexShrink: 1 }} />
                  </Stack>
                  <Stack>
                     <DetailsList
                        compact={true}
                        items={summaryItems}
                        columns={summaryColumns}
                        setKey="set"
                        layoutMode={DetailsListLayoutMode.justified}
                        isHeaderVisible={false}
                        selectionMode={SelectionMode.none}
                        onRenderItemColumn={onRenderSummaryItemColumn}
                     />
                  </Stack>
               </Stack>
               <Stack style={{ minWidth: 320 }}>
                  <Stack style={{ paddingBottom: 12 }}>
                     <Text variant="mediumPlus" styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>Workspaces</Text>
                  </Stack>
                  <DetailsList
                     compact={true}
                     items={workspaceItems}
                     columns={columns}
                     setKey="set"
                     layoutMode={DetailsListLayoutMode.justified}
                     isHeaderVisible={false}
                     selectionMode={SelectionMode.none}
                     onRenderItemColumn={onRenderItemColumn}
                  />
               </Stack>
               <AutoScalerPanel />
            </Stack>
         </Stack>
      </Stack>
   </Stack>)
};


export const PoolView: React.FC = observer(() => {

   const windowSize = useWindowSize();
   const { poolId } = useParams<{ poolId: string }>();

   useEffect(() => {

      handler.start();

      return () => {
         handler.clear();
      };

   }, []);

   // subscribe
   if (handler.updated) { };

   handler.set(poolId);

   const pool = handler.pool;

   const vw = Math.max(document.documentElement.clientWidth, window.innerWidth || 0);

   return <Stack className={hordeClasses.horde}>
      <TopNav />
      <Breadcrumbs items={[{ text: "Agents", link: "/agents" }, { text: `Pool ${handler.pool?.name ?? "Loading"}` }]} />
      <Stack horizontal>
         <div key={`windowsize_poolview_${windowSize.width}_${windowSize.height}`} style={{ width: vw / 2 - 896, flexShrink: 0, backgroundColor: modeColors.background }} />
         <Stack tokens={{ childrenGap: 0 }} styles={{ root: { backgroundColor: modeColors.background, width: "100%" } }}>
            <Stack style={{ maxWidth: 1778, paddingTop: 6, marginLeft: 4, height: 'calc(100vh - 8px)' }}>
               <Stack className={hordeClasses.raised}>
                  <Stack style={{ position: "relative", width: "100%", height: 'calc(100vh - 228px)' }}>
                     {!!pool && <ScrollablePane scrollbarVisibility={ScrollbarVisibility.always}>
                        <Stack tokens={{ childrenGap: 18 }}>
                           <PoolPanel />
                           <BatchPanel />
                           <StepPanel stepState={StepState.Active} />
                           <StepPanel stepState={StepState.Pending} />
                        </Stack>
                     </ScrollablePane>}
                     {!pool && <Spinner size={SpinnerSize.large} />}
                  </Stack>
               </Stack>
            </Stack>
         </Stack>
      </Stack>
   </Stack>
});

