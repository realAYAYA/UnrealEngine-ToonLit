// Copyright Epic Games, Inc. All Rights Reserved.

import { IconButton, IIconProps, Spinner, SpinnerSize, Stack, Text, Toggle } from '@fluentui/react';
import { IChartProps, ILineChartDataPoint } from '@fluentui/react-charting';
import { observer } from 'mobx-react-lite';
import moment from 'moment';
import React, { useCallback, useEffect, useState } from 'react';
import backend from '../../backend';
import { GetBatchResponse, GetJobTimingResponse, GetLabelResponse, GetLabelStateResponse, GetLabelTimingInfoResponse, GetNodeResponse, GetStepResponse, GetStepTimingInfoResponse, GetTemplateRefResponse, JobData, JobQuery, JobTimingsQuery, StepData } from '../../backend/Api';
import { JobLabel } from '../../backend/JobDetails';
import TemplateCache from '../../backend/TemplateCache';
import { HChartHoverCard } from '../../base/components/HordeFluentCharting/HHoverCard/HHoverCard';
import { HLineChart } from '../../base/components/HordeFluentCharting/HLineChart/HLineChart';
import { IHCustomizedCalloutData } from '../../base/components/HordeFluentCharting/HLineChart/HLineChart.types';
import { ISideRailLink } from '../../base/components/SideRail';
import { msecToElapsed } from '../../base/utilities/timeUtils';
import { hordeClasses } from '../../styles/Styles';
import { useQuery } from '../JobDetailCommon';
import { JobDataView, JobDetailsV2 } from './JobDetailsViewCommon';
import { copyToClipboard } from '../../base/utilities/clipboard';

const sideRail: ISideRailLink = { text: "Trends", url: "rail_trends" };

class TrendsDataView extends JobDataView {

   filterUpdated() {
      // this.updateReady();
   }

   set() {

   }

   clear() {
      super.clear();
      this.cacheData = undefined;
   }

   detailsUpdated() {

   }

   cacheData?: ChartDataCache;

   async populateChartDataCache(query: JobQuery): Promise<ChartDataCache> {
      if (this.cacheData) {
         return this.cacheData;
      }
      this.cacheData = { loading: true }
      const cacheData = await this._populateChartDataCache(query);
      this.cacheData = cacheData;
      this.updateReady();
      return cacheData;
   }

   private _populateChartDataCache(query: JobQuery): Promise<ChartDataCache> {
      return new Promise<ChartDataCache>((resolve, reject) => {
         function mapJobTimingToSteps(job: JobData, timing: GetJobTimingResponse, usedLabelIndicies: number[]) {

            const stepStarts = new Map<string, moment.Moment>();
            const stepFinishes = new Map<string, moment.Moment>();
            const batchStarts = new Map<string, moment.Moment>();
            const batchFinishes = new Map<string, moment.Moment>();

            job.batches?.forEach(b => {
               if (b.startTime) {
                  batchStarts.set(b.id, moment(b.startTime));
               }
               if (b.finishTime) {
                  batchFinishes.set(b.id, moment(b.finishTime));
               }
            });

            job.batches?.map(b => b.steps).flat().forEach((s) => {
               if (s!.startTime) {
                  stepStarts.set(s!.id, moment(s!.startTime));
               }
               if (s!.finishTime) {
                  stepFinishes.set(s!.id, moment(s!.finishTime));
               }
            });

            let steps: GraphHelperStep[] = [];
            function findBatchAndStep(stepId: string): [GetBatchResponse, GetStepResponse] | [undefined, undefined] {
               for (let batchIdx = 0; batchIdx < job.batches!.length; batchIdx++) {
                  let batch = job.batches![batchIdx];
                  let stepIdx = batch.steps.findIndex(step => step.id === stepId);
                  if (stepIdx !== -1) {
                     return [batch, batch.steps[stepIdx]];
                  }
               }
               return [undefined, undefined];
            }
            function findLabel(stepName: string): [number, GetLabelResponse, GetLabelStateResponse, GetLabelTimingInfoResponse] | [undefined, undefined, undefined, undefined] {
               for (let idx = 0; idx < usedLabelIndicies.length; idx++) {
                  let labelIdx = usedLabelIndicies[idx];
                  let graphLabel = job.graphRef?.labels?.[labelIdx];
                  if (graphLabel && graphLabel.includedNodes.includes(stepName)) {
                     return [labelIdx, graphLabel, job.labels![labelIdx], timing.labels[labelIdx]]
                  }
               }
               return [undefined, undefined, undefined, undefined];
            }
            function getNodeFromGraph(batch: GetBatchResponse, step: GetStepResponse): GetNodeResponse {
               return job.graphRef!.groups![batch.groupIdx].nodes[step.nodeIdx];
            };
            function calculateRunTime(batch: GetBatchResponse, step: GetStepResponse, stepTiming: GetStepTimingInfoResponse, node: GetNodeResponse): number {
               if (step.startTime && step.finishTime) {
                  let runTime = stepFinishes.get(step.id)!.diff(stepStarts.get(step.id)!, 'seconds');
                  return runTime;
               }
               return 0;
            };
            function calculateWaitTime(batch: GetBatchResponse, step: GetStepResponse, stepTiming: GetStepTimingInfoResponse, stepTimings: { [key: string]: GetStepTimingInfoResponse }): [number, number] {
               //return stepTiming.totalWaitTime!;

               let firstStep = batch.steps.reduce((prev, curr) => prev.nodeIdx < curr.nodeIdx ? prev : curr);
               if (step.nodeIdx === firstStep.nodeIdx) {
                  return [stepTiming.totalWaitTime!, stepTiming.totalWaitTime!];
               }
               else {
                  return [stepTiming.totalWaitTime! - stepTimings[firstStep.id].totalWaitTime!, stepTiming.totalWaitTime!];
               }
            };

            function calculateSetupTime(batch: GetBatchResponse, step: GetStepResponse, stepTiming: GetStepTimingInfoResponse, stepTimings: { [key: string]: GetStepTimingInfoResponse }): [number, number] {
               const start = batchStarts.get(batch.id)!;
               let end = moment(Date.now());
               let firstStep = batch.steps.reduce((prev, curr) => prev.nodeIdx < curr.nodeIdx ? prev : curr);

               if (!batch.steps.find(step => step.startTime) && batch.finishTime) {
                  end = batchFinishes.get(batch.id)!;
               } else {
                  batch.steps.forEach(step => {

                     if (!step.startTime) {
                        return;
                     }

                     const time = stepStarts.get(step.id)!;

                     if (time.unix() < end.unix()) {
                        end = time;
                     }
                  });
               }

               let result = end.diff(start, 'seconds');

               if (step.nodeIdx === firstStep.nodeIdx) {
                  return [result, result];
               }
               else {
                  return [0, result];
               }
            };

            // make a giant object with all of the proper dependencies for each step that ran as part of the timing data
            for (const [stepId, stepTiming] of Object.entries(timing.steps)) {
               // find the step key in the job groups
               let [batch, step] = findBatchAndStep(stepId);

               let [labelIdx, graphLabel, jobLabel, timingLabel] = findLabel(stepTiming.name!);
               if (batch && step) {
                  // get dependencies for the step from the graphref
                  let node = getNodeFromGraph(batch, step);
                  steps.push({
                     stepId: stepId,
                     name: stepTiming.name!,
                     labelIdx: labelIdx,
                     graphRefLabel: graphLabel,
                     jobLabel: jobLabel,
                     timingLabel: timingLabel,
                     job: job,
                     batch: batch,
                     step: step,
                     timing: stepTiming,
                     node: node,
                     setupTime: calculateSetupTime(batch, step, stepTiming, timing.steps),
                     runTime: calculateRunTime(batch, step, stepTiming, node),
                     waitTime: calculateWaitTime(batch, step, stepTiming, timing.steps),
                     dependencies: [],
                     price: undefined, //getJobStepPrice(batch, step),
                     jobPrice: undefined // getjobPrice(job)
                  });
               }
            }
            return steps;
         };

         const stepsByName: Map<string, GraphHelperStep> = new Map();
         const stepsByLogs: Map<string, GraphHelperStep> = new Map();

         function mapStepDependencies(allSteps: GraphHelperStep[], inputStep: GraphHelperStep, outputSteps: Set<GraphHelperStep>) {

            if (!stepsByName.size) {
               allSteps.forEach(s => {
                  stepsByName.set(s.name, s);
                  if (s.step.logId) {
                     stepsByLogs.set(s.step.logId, s);
                  }
               })
            }


            for (let idx = 0; idx < inputStep.node.orderDependencies.length; idx++) {
               let dependencyStepName = inputStep.node.orderDependencies[idx];
               let dependencyStep = stepsByName.get(dependencyStepName);
               if (dependencyStep) {
                  mapStepDependencies(allSteps, dependencyStep, outputSteps);
                  outputSteps.add(dependencyStep);
               }
               inputStep.batch.steps.forEach(batchDependencyStep => {
                  if (batchDependencyStep.state === "Completed" && (batchDependencyStep.outcome === "Success" || batchDependencyStep.outcome === "Warnings") && batchDependencyStep.nodeIdx < inputStep.step.nodeIdx) {
                     if (batchDependencyStep.logId) {
                        const batchStepHelper = stepsByLogs.get(batchDependencyStep.logId);
                        if (batchStepHelper) {
                           outputSteps.add(batchStepHelper);
                        }
                     }
                  }
               });
            }
         }

         backend.getBatchJobTiming(query).then(timingsDict => {
            let chartData: ChartData = { steps: {}, labelData: { "default": [] } };
            let minChange = Number.MAX_VALUE;
            for (const [, timing] of Object.entries(timingsDict.timings)) {
               let job = timing.jobResponse;
               minChange = Math.min(job.change!, minChange);
               // make a map of executed labels so we can assign them to steps
               let usedLabelIndicies = [];
               for (let idx = 0; idx < job.labels!.length; idx++) {
                  if (job.labels![idx].state === "Complete") {
                     usedLabelIndicies.push(idx);
                  }
               }
               // calculate all of the timing steps
               let steps = mapJobTimingToSteps(job, timing, usedLabelIndicies);
               steps.forEach(step => {
                  const outputSteps: Set<GraphHelperStep> = new Set<GraphHelperStep>();
                  mapStepDependencies(steps, step, outputSteps);
                  step.dependencies = Array.from(outputSteps);
               });

               chartData.steps![job.change!] = { change: job.change!, date: moment(job.createTime), steps: steps };
               // build a dictionary of labels to steps
               let defaultLabelItem: GraphHelperLabel = { change: job.change!, date: moment(job.createTime), steps: [] };
               let defaultLabel = job.defaultLabel;
               if (defaultLabel) {
                  defaultLabel.nodes.forEach(node => {
                     // check each step to see if we have timing data for it
                     let step = steps.find(step => step.name === node);
                     if (step) {
                        defaultLabelItem.steps.push(step);
                     }
                  });
               }
               chartData.labelData["default"].push(defaultLabelItem);
               // loop through the rest of the labels and assign steps to them
               usedLabelIndicies.forEach(labelIdx => {
                  let label = timing.labels[labelIdx];
                  let labelSteps = new Set<GraphHelperStep>();
                  steps.filter(step => step.labelIdx === labelIdx).forEach(step => {
                     labelSteps.add(step);
                     if (step.graphRefLabel) {
                        step.graphRefLabel.includedNodes.forEach(includedStepName => {
                           let includedStep = steps.find(step => step.name === includedStepName)!;
                           if (includedStep)
                              labelSteps.add(includedStep);
                        });
                     }
                  });
                  let labelItem: GraphHelperLabel = { change: job.change!, date: moment(job.createTime), steps: Array.from(labelSteps) };
                  let chartDataKey = `${label.category}-${label.name}`;
                  if (!chartData.labelData[chartDataKey])
                     chartData.labelData[chartDataKey] = [];
                  chartData.labelData[chartDataKey].push(labelItem);
               });
            };

            resolve({ loading: false, chartData: chartData });
         });
      });
   };

   order = 8;
}

JobDetailsV2.registerDataView("TrendsDataView", (details: JobDetailsV2) => new TrendsDataView(details));

const colorSteps = [
   "#2e8b57",
   "#483d8b",
   "#b03060",
   "#ffa500",
   "#00bfff",
   "#696969",
   "#8b4513",
   "#00008b",
   "#808000",
   "#008000",
   "#9acd32",
   "#7f007f",
   "#ff0000",
   "#ffff00",
   "#40e0d0",
   "#7fff00",
   "#8a2be2",
   "#00ff7f",
   "#dc143c",
   "#f4a460",
   "#0000ff",
   "#f08080",
   "#b0c4de",
   "#ff00ff",
   "#1e90ff",
   "#eee8aa",
   "#90ee90",
   "#ff1493",
   "#7b68ee",
   "#ee82ee"
];


type GraphHelperStep = {
   stepId: string,
   name: string,
   labelIdx: number | undefined,
   graphRefLabel: GetLabelResponse | undefined,
   jobLabel: GetLabelStateResponse | undefined,
   timingLabel: GetLabelTimingInfoResponse | undefined,
   job: JobData,
   batch: GetBatchResponse,
   step: GetStepResponse
   timing: GetStepTimingInfoResponse,
   node: GetNodeResponse,
   setupTime: [number | undefined, number | undefined]
   runTime: number | undefined,
   waitTime: [number | undefined, number | undefined]
   dependencies: GraphHelperStep[] | undefined
   price: number | undefined
   jobPrice: number | undefined
}

type GraphHelperLabel = {
   change: number
   date: moment.Moment
   steps: GraphHelperStep[]
}

type ChartData = {
   labelData: { [labelId: string]: GraphHelperLabel[] };
   steps: { [change: number]: GraphHelperLabel } | undefined;
};

type FilteredChartData = {
   chartProps: IChartProps | undefined;
   yMaxValue: number;
   leftMargin: number;
};

type ChartDataCache = {
   loading: boolean;
   chartData?: ChartData;
}

type PageInfo = {
   pageNumber: number;
   zoomLevel: number;
   pageSize: number;
}

function preventChartScroll(this: Document, e: WheelEvent) {
   e = e || window.event
   if (e.preventDefault) {
      e.preventDefault()
   }
   e.returnValue = false
}

export const StepTrendsPanel: React.FC<{ jobDetails: JobDetailsV2 }> = observer(({ jobDetails }) => {
   const query = useQuery();

   const stepId = query.get("step") ? query.get("step")! : undefined;
   const labelIdx = query.get("label") ? query.get("label")! : undefined;

   const dataView = jobDetails.getDataView<TrendsDataView>("TrendsDataView");

   useEffect(() => {
      return () => {
         dataView?.clear();
      };
   }, [dataView]);

   dataView.subscribe();

   if (jobDetails?.jobData) {
      dataView.initialize(jobDetails.jobData?.preflightChange ? undefined : [sideRail]);
   } else {
      jobDetails?.subscribe();
   }

   if (jobDetails.jobData == null || !jobDetails?.viewsReady || jobDetails.jobData.preflightChange) {
      return null;
   }

   if (jobDetails.jobData?.preflightChange) {
      return null;
   }

   const step = jobDetails.stepById(stepId);
   const label = jobDetails.labelByIndex(labelIdx);

   if (stepId !== undefined && !step) {
      return null;
   }

   if (labelIdx !== undefined && !label) {
      return null;
   }

   return <Stack id={sideRail.url}>
      <StepTrendsPanelInner jobDetails={jobDetails} dataView={dataView} step={step} label={label} />
   </Stack>
});

const StepTrendsPanelInner: React.FC<{ jobDetails: JobDetailsV2; dataView: TrendsDataView, label?: JobLabel, batch?: GetBatchResponse, step?: StepData }> = observer(({ jobDetails, dataView, label, batch, step }) => {
   const [selectedTemplateRef, setSelectedTemplateRef] = useState<GetTemplateRefResponse | undefined>(undefined);
   const [includeWaitTimes, setIncludeWaitTimes] = useState<boolean>(false);
   const [includeStepDependencies, setIncludeStepDependencies] = useState<boolean>(false);
   const [copyCLToClipboard, setCopyCLToClipboard] = useState<string | undefined>(undefined);

   const [pageInfo, setPageInfo] = useState<PageInfo>({ pageNumber: 1, zoomLevel: 10, pageSize: 10 });

   dataView.subscribe();

   const findTemplate = useCallback(async () => {
      if (jobDetails.stream) {
         TemplateCache.getStreamTemplates(jobDetails.stream).then(templates => {
            templates.forEach(template => {
               if (template.id === jobDetails.template?.id || template.name === jobDetails.template?.name) {
                  setSelectedTemplateRef(template);
                  return;
               }
            });
         });
      }
   }, [jobDetails]);


   if (!selectedTemplateRef) {
      findTemplate();
      return null;
   }

   // @todo: this view needs a reboot
   let count = selectedTemplateRef.id?.indexOf("incremental") === -1 ? 150 : 100;
   if (step) {
      count *= 2;
   }

   if (!dataView.cacheData) {
      const query: JobTimingsQuery = {
         streamId: jobDetails!.stream!.id,
         template: [selectedTemplateRef.id],
         count: count
      };
      dataView.populateChartDataCache(query);
   }

   if (dataView.cacheData?.chartData) {
      let totalValues = Object.values(dataView.cacheData.chartData!.labelData).map(c => c.length).reduce((a, b) => Math.max(a, b));
      pageInfo.pageSize = totalValues / 10;
   }



   const filterChartData = (chartDataCache?: ChartDataCache) => {
      if (!chartDataCache) {
         return undefined;
      }

      function sortCalloutData(calloutData: any) {
         let sortedCallouts = Object.entries(calloutData).sort(([, a], [, b]) => (a as any).startTime.diff((b as any).startTime));
         let newCalloutData: any = {};
         sortedCallouts.forEach(callout => {
            newCalloutData[callout[0]] = (callout[1] as any).label;
         });
         return newCalloutData;
      }

      let filteredChartData: FilteredChartData = { chartProps: undefined, leftMargin: 40, yMaxValue: 0 };
      let newChartProps: IChartProps = { lineChartData: [] };
      let colorStep = 0;
      let yMax = 0;
      let setupTimeDate = new Date(863999999999998);
      let waitTimeDate = new Date(863999999999999);
      let totalTimeDate = new Date(8640000000000000);
      let allDates: any = [];

      let numPoints = pageInfo.zoomLevel * pageInfo.pageSize;
      if (chartDataCache && chartDataCache.loading === false) {
         let filteredChartItem = chartDataCache.chartData!;
         if (step && step.timing && step.timing.name && filteredChartItem.steps) {
            let labelChartPoints: ILineChartDataPoint[] = [];
            let stepChanges = Object.keys(filteredChartItem.steps);

            let numChanges = Math.min(numPoints, stepChanges.length);
            let idxStart = Math.max(0, stepChanges.length - numChanges * pageInfo.pageNumber);
            let idxEnd = Math.min(stepChanges.length - numChanges * (pageInfo.pageNumber - 1), stepChanges.length);
            for (let changeIdx = idxStart; changeIdx < idxEnd; changeIdx++) {
               const change = stepChanges[changeIdx];
               const helperData = filteredChartItem.steps[parseInt(stepChanges[changeIdx])];
               let dataStep = helperData.steps.find(s => s.name === step.timing!.name);

               if (dataStep && (dataStep.step.outcome === "Success" || dataStep.step.outcome === "Warnings") && dataStep.step.state === "Completed") {
                  let stepRunTime = dataStep.runTime! * 1000;
                  let stepWaitTime = (includeStepDependencies ? dataStep.waitTime[0]! : dataStep.waitTime[1]!) * 1000;
                  let stepSetupTime = (includeStepDependencies ? dataStep.setupTime[0]! : dataStep.setupTime[1]!) * 1000;

                  let calloutData: any = {};
                  calloutData[dataStep.name] = { startTime: moment(dataStep.step.startTime!), label: msecToElapsed(stepRunTime) };
                  if (includeWaitTimes) {
                     stepRunTime += stepWaitTime;
                     stepRunTime += stepSetupTime;
                     calloutData['Wait Time'] = { startTime: moment(waitTimeDate), label: stepWaitTime };
                     calloutData['Setup Time'] = { startTime: moment(setupTimeDate), label: stepSetupTime };
                  }

                  if (includeStepDependencies && dataStep.dependencies) {
                     dataStep.dependencies.forEach(inputStep => {
                        let inputStepRunTime = inputStep.runTime! * 1000;
                        let inputWaitTime = inputStep.waitTime[0]! * 1000;
                        let inputSetupTime = inputStep.setupTime[0]! * 1000;
                        calloutData[inputStep.name] = { startTime: moment(inputStep.step.startTime), label: msecToElapsed(inputStepRunTime) };
                        stepRunTime += inputStepRunTime;

                        if (includeWaitTimes) {
                           stepRunTime += inputWaitTime;
                           stepRunTime += inputSetupTime;
                           calloutData['Wait Time'].label += inputWaitTime;
                           calloutData['Setup Time'].label += inputSetupTime;
                        }

                     });
                  }
                  if ('Wait Time' in calloutData) {
                     calloutData['Wait Time'].label = msecToElapsed(calloutData['Wait Time'].label);
                  }

                  if ('Setup Time' in calloutData) {
                     calloutData['Setup Time'].label = msecToElapsed(calloutData['Setup Time'].label);
                  }

                  if ('Wait Time' in calloutData || includeStepDependencies) {
                     calloutData['Total Time'] = { startTime: moment(totalTimeDate), label: msecToElapsed(stepRunTime) };
                  }

                  calloutData = sortCalloutData(calloutData);

                  yMax = Math.max(yMax, stepRunTime);
                  allDates.push(helperData.date);
                  labelChartPoints.push({
                     x: helperData.date.toDate(),
                     y: stepRunTime,
                     yAxisCalloutData: calloutData,
                     xAxisCalloutData: `CL ${change}`,
                     onDataPointClick: () => {
                        copyToClipboard(change); setCopyCLToClipboard(change); setTimeout(() => {
                           setCopyCLToClipboard(undefined);
                        }, 3000)
                     }
                  });
               }
            }
            // add data as long as there's something relevant to display
            if (!labelChartPoints.every(point => point.y === 0)) {
               let existingProps = newChartProps.lineChartData!.find(existing => existing.legend === step.timing!.name);
               if (existingProps) {
                  existingProps.data = existingProps.data.concat(labelChartPoints);
               }
               else {
                  newChartProps.lineChartData!.push({
                     color: colorSteps[colorStep],
                     data: labelChartPoints,
                     legend: step.timing.name
                  });
                  colorStep++;
                  if (colorStep === colorSteps.length) {
                     colorStep = 0;
                  }
               }
            }
         }
         else {
            let labels = Object.entries(chartDataCache.chartData!.labelData);
            if (label) {
               let key = `${label.category}-${label.name}`;
               if (key === "Other-Other") {
                  key = "default";
               }
               labels = [[key, chartDataCache.chartData!.labelData[key]]];
            }

            const changesToAllStepsMap: { [change: number]: Set<GraphHelperStep> } = {};
            for (const [labelChange, labelChanges] of labels) {
               if (labelChange !== "undefined-undefined" && labelChanges) {
                  for (let changeIdx = 0; changeIdx < labelChanges.length; changeIdx++) {
                     let changeLabel = labelChanges[changeIdx];
                     if (!changesToAllStepsMap[changeLabel.change]) {
                        changesToAllStepsMap[changeLabel.change] = new Set<GraphHelperStep>();
                     }
                     let change = labelChanges[changeIdx];
                     let steps = change.steps;
                     if (steps.length) {
                        if (steps.every(step => (step.step.outcome === "Success" || step.step.outcome === "Warnings") && step.step.state === "Completed")) {
                           steps.forEach(step => {
                              changesToAllStepsMap[changeLabel.change].add(step);
                           });
                        }
                     }
                  }
               }
            }

            for (const [labelChange, labelChanges] of labels) {
               if (labelChange !== "undefined-undefined") {
                  let labelChartPoints: ILineChartDataPoint[] = [];

                  if (!labelChanges?.length) {
                     continue;
                  }

                  let numChanges = Math.min(numPoints, labelChanges.length);
                  let idxStart = numChanges * (pageInfo.pageNumber - 1);
                  let idxEnd = Math.min(numChanges * pageInfo.pageNumber, labelChanges.length);
                  for (let changeIdx = idxStart; changeIdx < idxEnd; changeIdx++) {
                     let change = labelChanges[changeIdx];
                     let steps = change.steps;
                     if (steps.length) {
                        if (steps.every(step => (step.step.outcome === "Success" || step.step.outcome === "Warnings") && step.step.state === "Completed")) {
                           // get the overall
                           let labelRunTime = 0;
                           let calloutData: any = {};
                           // if on the jobdetails view (so no label), just get aggregate label times for the graph lines
                           if (!label) {
                              labelRunTime = steps.map(step => { return step.runTime! }).reduce((a, b) => a + b, 0) * 1000;
                              calloutData[labelChange] = { startTime: moment(0), label: msecToElapsed(labelRunTime) };

                              if (includeWaitTimes) {
                                 if (includeWaitTimes) {
                                    calloutData['Total Time'] = { startTime: moment(totalTimeDate), label: labelRunTime };
                                    calloutData['Wait Time'] = { startTime: moment(waitTimeDate), label: 0 };
                                    calloutData['Setup Time'] = { startTime: moment(setupTimeDate), label: 0 };
                                 }
                                 steps.forEach(step => {
                                    if (includeWaitTimes) {
                                       let waitTime = step.waitTime[0]! * 1000;
                                       let setupTime = step.setupTime[0]! * 1000;
                                       labelRunTime += waitTime;
                                       labelRunTime += setupTime;
                                       calloutData['Wait Time'].label += waitTime;
                                       calloutData['Total Time'].label += waitTime;

                                       calloutData['Setup Time'].label += setupTime;
                                       calloutData['Total Time'].label += setupTime;
                                    }
                                 });
                              }

                           }
                           // otherwise get the individual steps
                           else {
                              calloutData['Total Time'] = { startTime: moment(totalTimeDate), label: 0 };
                              if (includeWaitTimes) {
                                 calloutData['Wait Time'] = { startTime: moment(waitTimeDate), label: 0 };
                                 calloutData['Setup Time'] = { startTime: moment(setupTimeDate), label: 0 };
                              }
                              steps.forEach(step => {
                                 let runTime = step.runTime! * 1000;
                                 calloutData[step.name] = { startTime: moment(step.step.startTime!), label: msecToElapsed(runTime) };

                                 if (includeWaitTimes) {
                                    let waitTime = step.waitTime[0]! * 1000;
                                    let setupTime = step.setupTime[0]! * 1000;
                                    runTime += waitTime;
                                    runTime += setupTime;
                                    calloutData['Wait Time'].label += waitTime;
                                    calloutData['Setup Time'].label += setupTime;
                                 }
                                 calloutData['Total Time'].label += runTime;
                                 labelRunTime += runTime;

                              });
                           }

                           yMax = Math.max(yMax, labelRunTime);

                           if ('Wait Time' in calloutData) {
                              calloutData['Wait Time'].label = msecToElapsed(calloutData['Wait Time'].label);
                           }
                           if ('Setup Time' in calloutData) {
                              calloutData['Setup Time'].label = msecToElapsed(calloutData['Setup Time'].label);
                           }
                           if ('Total Time' in calloutData) {
                              calloutData['Total Time'].label = msecToElapsed(calloutData['Total Time'].label);
                           }

                           calloutData = sortCalloutData(calloutData);

                           allDates.push(change.date);
                           labelChartPoints.push({
                              x: change.date.toDate(),
                              y: labelRunTime,
                              yAxisCalloutData: calloutData,
                              xAxisCalloutData: `CL ${change.change}`,
                           });
                        }
                     }
                  }
                  // add data as long as there's something relevant to display
                  if (!labelChartPoints.every(point => point.y === 0)) {
                     let existingProps = newChartProps.lineChartData!.find(existing => existing.legend === labelChange);
                     if (existingProps) {
                        existingProps.data = existingProps.data.concat(labelChartPoints);
                     }
                     else {
                        newChartProps.lineChartData!.push({
                           color: colorSteps[colorStep],
                           data: labelChartPoints,
                           legend: labelChange
                        });
                        colorStep++;
                        if (colorStep === colorSteps.length) {
                           colorStep = 0;
                        }
                     }
                  }
               }
            }
         }

         // margins aren't recalcluating when the tick values change, annoyingly.
         let leftMargin = 40;
         const yMaxValue = Math.ceil(yMax / 300000) * 300000;
         if (yMaxValue > 122400000) {
            leftMargin = 80;
         }
         else if (yMaxValue > 36000000) {
            leftMargin = 60;
         }
         else if (yMaxValue > 86400000) {
            leftMargin = 70;
         }
         else if (yMaxValue > 3600000) {
            leftMargin = 50;
         }

         filteredChartData.leftMargin = Math.max(filteredChartData.leftMargin ?? 0, leftMargin);
         filteredChartData.yMaxValue = Math.max(filteredChartData.yMaxValue ?? 0, yMaxValue);

         // filter out default label if applicable
         if (newChartProps.lineChartData) {
            // do something (convert to steps?) if we only have a default label
            if (newChartProps.lineChartData.length === 1 && newChartProps.lineChartData[0].legend === "default") {

            }
            // otherwise cut it out
            else {
               newChartProps.lineChartData = newChartProps.lineChartData!.filter(item => {
                  return item.legend !== 'default'
               });
            }
         }

         // sort all the arbitrary points we may have added to each of the props
         newChartProps.lineChartData!.forEach(chartLine => {
            chartLine.data.sort((a, b) => { return (a.x as Date).getTime() - (b.x as Date).getTime() });
         });

         filteredChartData.chartProps = newChartProps;
      }
      return filteredChartData;
   };

   function yAxisFormatter(seconds?: any) {
      return msecToElapsed(seconds);
   }

   let filteredChartState: FilteredChartData | undefined = filterChartData(dataView.cacheData);


   let placeholderDiv = <div></div>;
   let titleText = 'Trends';
   if (filteredChartState && filteredChartState?.chartProps) {
      if (filteredChartState.chartProps?.lineChartData?.length) {
         placeholderDiv = <HLineChart
            data={filteredChartState.chartProps}
            hideLegend={false}
            legendProps={{ canSelectMultipleLegends: true, allowFocusOnLegends: false, enabledWrapLines: true }}
            tickFormat={'%a %m/%e %I%p'}
            yAxisTickFormat={yAxisFormatter}
            margins={{ top: 20, bottom: 35, left: filteredChartState.leftMargin, right: 45 }}
            yAxisTickCount={5}
            yMaxValue={filteredChartState.yMaxValue}
            onRenderCalloutPerDataPoint={(props: (IHCustomizedCalloutData | undefined)[] | undefined) => {
               if (!props || (!props[0] && !props[1])) {
                  return null;
               }

               let hoverPoint1 = props[0];
               let hoverPoint2 = props[1];

               let dataPoint1 = hoverPoint1 ? hoverPoint1.values.find((point: { legend: string | number; y: string | number }) => point.y === hoverPoint1!.hoverYValue) : undefined;
               let dataPoint2 = hoverPoint2 ? hoverPoint2.values.find((point: { legend: string | number; y: string | number }) => point.y === hoverPoint2!.hoverYValue) : undefined;

               if (hoverPoint1 && dataPoint1) {
                  return <HChartHoverCard props1={hoverPoint1} dataPoint1={dataPoint1} props2={hoverPoint2} dataPoint2={dataPoint2} />
               }
               return null;
            }}
         />
      }
      else {
         placeholderDiv = <div>No trends data yet.</div>;
      }
   }

   const onChartScroll = (e: any) => {
      let newPageInfo = Object.assign({}, pageInfo);
      const direction = Math.sign(e.deltaY);
      if (direction > 0 && newPageInfo.zoomLevel < 10) {
         newPageInfo.zoomLevel++;
         newPageInfo.pageNumber = Math.max(1, newPageInfo.pageNumber - 1);
      }
      else if (direction < 0 && newPageInfo.zoomLevel > 1) {
         newPageInfo.zoomLevel--;
      }
      setPageInfo(newPageInfo);
   };

   const onPageChange = (direction: number) => {
      let newPageInfo = Object.assign({}, pageInfo);
      newPageInfo.pageNumber += direction;
      let maxPage = Math.ceil(newPageInfo.pageSize / newPageInfo.zoomLevel);
      console.log(`maxPage: ${maxPage}`);
      if (newPageInfo.pageNumber < 1) {
         newPageInfo.pageNumber = 1;
      }
      else if (newPageInfo.pageNumber > maxPage) {
         newPageInfo.pageNumber = maxPage;
      }
      setPageInfo(newPageInfo);

   };

   const onChartWheelEnter = (e: any) => {
      document.addEventListener('wheel', preventChartScroll, { passive: false })
   };

   const onChartWheelLeave = (e: any) => {
      document.removeEventListener('wheel', preventChartScroll, false)
   };

   let spinnerObj: any = <Spinner styles={{ root: { display: 'inline-block', position: 'relative', top: -10 } }} size={SpinnerSize.large} />
   let statusText: string = "Fetching template data...";

   if (selectedTemplateRef) {
      statusText = `Fetching trends data...`;
   }

   const nextIcon: IIconProps = { iconName: 'Forward' };
   const backIcon: IIconProps = { iconName: 'Back' };

   let statusDiv = <div>{spinnerObj} <Text styles={{ root: { position: 'relative', top: -18, left: 5 } }}> {statusText} </Text></div>;
   if (dataView.cacheData && !dataView.cacheData.loading && filteredChartState) {
      let totalValues = Object.values(dataView.cacheData.chartData!.labelData).map(c => c.length).reduce((a, b) => Math.max(a, b));
      let minShown = ((pageInfo.pageNumber - 1) * pageInfo.pageSize * pageInfo.zoomLevel) + 1;
      let maxShown = Math.min(totalValues, Math.min(totalValues, pageInfo.pageNumber * pageInfo.pageSize) * pageInfo.zoomLevel);

      statusText = `Showing ${minShown}-${maxShown} of ${totalValues}`;

      statusDiv = (<div>
         <IconButton styles={{ root: { position: 'relative', top: -18, left: -4 } }} iconProps={backIcon} onClick={() => { onPageChange(1); }} />
         <IconButton styles={{ root: { position: 'relative', top: -18, left: -4 } }} iconProps={nextIcon} onClick={() => { onPageChange(-1) }} />
         <Text styles={{ root: { position: 'relative', top: -23, left: 0 } }}> {statusText} </Text>
      </div>);
   }

   return (<Stack styles={{ root: { paddingTop: 18, paddingRight: 12 } }}>
      <Stack className={hordeClasses.raised}>
         <Stack>
            <Stack horizontal horizontalAlign="space-between" styles={{ root: { minHeight: 32, paddingRight: 8 } }}>
               <Text variant="mediumPlus" styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>{titleText}</Text>
               {!!step && <Toggle label="Include dependencies" checked={includeStepDependencies} onText="On" offText="Off" onChange={() => setIncludeStepDependencies(!includeStepDependencies)} />}
               <Toggle label="Include wait times" checked={includeWaitTimes} onText="On" offText="Off" onChange={() => setIncludeWaitTimes(!includeWaitTimes)} />
            </Stack>
            <Stack styles={{ root: { position: 'relative', top: -10 } }}>
               {statusDiv}
            </Stack>
            <Stack styles={{ root: { width: 1370, minHeight: 400, height: 400, position: "relative" } }} onWheel={onChartScroll} onMouseEnter={onChartWheelEnter} onMouseLeave={onChartWheelLeave}>
               {!!copyCLToClipboard && <Stack style={{ position: "absolute", top: -12, left: 580 }}>
                  <Text variant="mediumPlus">Copied CL {copyCLToClipboard} to the clipboard</Text>
               </Stack>}
               {placeholderDiv}
            </Stack>

         </Stack>

      </Stack></Stack>);
});
