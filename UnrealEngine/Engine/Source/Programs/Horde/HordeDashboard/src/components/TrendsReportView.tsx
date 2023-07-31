// Copyright Epic Games, Inc. All Rights Reserved.

import { ContextualMenu, Dialog, DialogType, IconButton, IIconProps, Stack, Text  } from '@fluentui/react';
import React, { useCallback, useEffect, useState } from 'react';
import { hordeClasses } from '../styles/Styles';
import backend from '../backend';
import { IChartProps, ILineChartDataPoint } from '@fluentui/react-charting';
import { HLineChart } from '../base/components/HordeFluentCharting/HLineChart/HLineChart';
import { mergeStyleSets, Spinner, SpinnerSize, Toggle } from '@fluentui/react';
import TemplateCache from '../backend/TemplateCache';
import { GetBatchResponse, GetJobTimingResponse, GetLabelResponse, GetLabelStateResponse, GetLabelTimingInfoResponse, GetNodeResponse, GetStepResponse, GetStepTimingInfoResponse, GetTemplateRefResponse, JobData, JobQuery, JobState, JobTimingsQuery, StepData } from '../backend/Api';
import { JobDetails, JobLabel } from '../backend/JobDetails';
import { msecToElapsed } from '../base/utilities/timeUtils';
import { HChartHoverCard } from '../base/components/HordeFluentCharting/HHoverCard/HHoverCard';
import moment from 'moment';
import { IHCustomizedCalloutData } from '../base/components/HordeFluentCharting/HLineChart/HLineChart.types';

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
	setupTime: [ number | undefined, number | undefined ]
	runTime: number | undefined,
	waitTime: [ number | undefined, number | undefined ]
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
	labelData: { [labelId: string] : GraphHelperLabel[] };
	steps: { [change: number] : GraphHelperLabel } | undefined;
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

type XRayBatch = {
	dependencyWaitTime: number;
	waitTime: number;
	setupTime: number;
	reserveTime: moment.Moment;
	startTime: moment.Moment;
	finishTime: moment.Moment;
	steps: GraphHelperStep[];
}

type XRayColumn = { 
	minX: number;
	maxX: number;
	batches: XRayBatch[];
}

type XRayCanvasData = {
	jobStartTime: moment.Moment;
	jobEndTime: moment.Moment;
	minX: number;
	maxX: number;
	minY: number;
	maxY: number;
	canvasWidth: number;
	canvasHeight: number;
	columns: XRayColumn[];
	change: number;
}

type PageInfo = {
	pageNumber: number;
	zoomLevel: number;
	pageSize: number;
}

let xRayHoveredJobStep: GraphHelperStep | undefined = undefined;
function preventChartScroll(this: Document, e: WheelEvent) {
	e = e || window.event
	if (e.preventDefault) {
	  e.preventDefault()
	}
	e.returnValue = false
}

export const TrendsReportView: React.FC<{ jobDetails: JobDetails; label?: JobLabel, batch?: GetBatchResponse, step?: StepData }> = ({ jobDetails, label, batch, step }) => {
	const [selectedTemplateRef, setSelectedTemplateRef] = useState<GetTemplateRefResponse|undefined>(undefined);
	const [cacheData, setCacheData] = useState<ChartDataCache|undefined>(undefined);
	const [includeWaitTimes, setIncludeWaitTimes] = useState<boolean>(false);
	const [includeStepDependencies, setIncludeStepDependencies] = useState<boolean>(false);
	const [includeCost, setIncludeCost] = useState<boolean>(false);
	
	const [xRayData, setXRayData] = useState<XRayCanvasData|undefined>(undefined);
	const [canvas, setCanvas] = useState<HTMLCanvasElement | null>(null);

	const [pageInfo, setPageInfo] = useState<PageInfo>({ pageNumber: 1, zoomLevel: 1, pageSize: 10});

	const populateChartDataCache = useCallback((query: JobQuery): Promise<ChartDataCache> => {
		return new Promise<ChartDataCache>((resolve, reject) => {
			function mapJobTimingToSteps(job: JobData, timing: GetJobTimingResponse, usedLabelIndicies: number[]) {
				let steps: GraphHelperStep[] = [];
				function findBatchAndStep(stepId: string): [ GetBatchResponse, GetStepResponse ] | [ undefined, undefined ] {
					for(let batchIdx = 0; batchIdx < job.batches!.length; batchIdx++) {
						let batch = job.batches![batchIdx];
						let stepIdx = batch.steps.findIndex(step => step.id === stepId);
						if(stepIdx !== -1) {
							return [ batch, batch.steps[stepIdx] ];
						}
					}
					return [ undefined, undefined ];
				}
				function findLabel(stepName: string): [ number, GetLabelResponse, GetLabelStateResponse, GetLabelTimingInfoResponse ] | [ undefined, undefined, undefined, undefined ] {
					for(let idx = 0; idx < usedLabelIndicies.length; idx++) {
						let labelIdx = usedLabelIndicies[idx];
						let graphLabel = job.graphRef?.labels?.[labelIdx];
						if(graphLabel && graphLabel.includedNodes.includes(stepName)) {
							return [ labelIdx, graphLabel, job.labels![labelIdx], timing.labels[labelIdx] ]
						}
					}
					return [ undefined, undefined, undefined, undefined ];
				}
				function getNodeFromGraph(batch: GetBatchResponse, step: GetStepResponse): GetNodeResponse {
					return job.graphRef!.groups![batch.groupIdx].nodes[step.nodeIdx];
				};
				function calculateRunTime(batch: GetBatchResponse, step: GetStepResponse, stepTiming: GetStepTimingInfoResponse, node: GetNodeResponse): number {
					if(step.startTime && step.finishTime) {
						let runTime = moment(step.finishTime).diff(step.startTime, 'seconds');
						return runTime;
					}
					return 0;
				};
				function calculateWaitTime(batch: GetBatchResponse, step: GetStepResponse, stepTiming: GetStepTimingInfoResponse, stepTimings: { [key: string]: GetStepTimingInfoResponse }): [ number, number ] {
					//return stepTiming.totalWaitTime!;
					
					let firstStep = batch.steps.reduce((prev, curr) => prev.nodeIdx < curr.nodeIdx ? prev : curr);
					if(step.nodeIdx === firstStep.nodeIdx) {
						return [ stepTiming.totalWaitTime!, stepTiming.totalWaitTime! ];
					}
					else {
						return [ stepTiming.totalWaitTime! - stepTimings[firstStep.id].totalWaitTime!, stepTiming.totalWaitTime! ];
					}
				};

				function calculateSetupTime(batch: GetBatchResponse, step: GetStepResponse, stepTiming: GetStepTimingInfoResponse, stepTimings: { [key: string]: GetStepTimingInfoResponse }): [ number, number ] {
					const start = moment(batch.startTime);
					let end = moment(Date.now());
					let firstStep = batch.steps.reduce((prev, curr) => prev.nodeIdx < curr.nodeIdx ? prev : curr);
				
					if (!batch.steps.find(step => step.startTime) && batch.finishTime) {
						end = moment(batch.finishTime);
					} else {
						batch.steps.forEach(step => {
				
							if (!step.startTime) {
								return;
							}
				
							const time = moment(step.startTime);
				
							if (time.unix() < end.unix()) {
								end = time;
							}
						});
					}

					let result = end.diff(start, 'seconds');
					
					if(step.nodeIdx === firstStep.nodeIdx) {
						return [ result, result ];
					}
					else {
						return [ 0, result ];
					}
				};

				function getJobStepPrice(batch: GetBatchResponse, step: GetStepResponse): number | undefined  {
					if (!step || !batch || !batch.agentRate || !step.startTime || !step.finishTime) {
						return undefined;
					}
			
					const start = moment(step.startTime);
					const end = moment(step.finishTime);
					const hours = moment.duration(end.diff(start)).asHours();
					const price = hours * batch.agentRate;
											
					return price ? price : undefined;
				}
			
				function getjobPrice(job: JobData): number | undefined {
					if (!job.batches || job.state !== JobState.Complete) {
						return undefined;
					}
			
					let price = 0;
					job.batches.forEach(b => {
						if (b.agentRate && b.startTime && b.finishTime) {
							const start = moment(b.startTime);
							const end = moment(b.finishTime);
							const hours = moment.duration(end.diff(start)).asHours();
							price += hours * b.agentRate;
											
						} 
					});
			
					return price ? price : undefined;
				}
	
				// make a giant object with all of the proper dependencies for each step that ran as part of the timing data
				for(const [stepId, stepTiming] of Object.entries(timing.steps)) {
					// find the step key in the job groups
					let [batch, step] = findBatchAndStep(stepId);
	
					let [ labelIdx, graphLabel, jobLabel, timingLabel ] = findLabel(stepTiming.name!);
					if(batch && step) {
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

			function mapStepDependencies(allSteps: GraphHelperStep[], inputStep: GraphHelperStep, outputSteps: Set<GraphHelperStep>) {
				for(let idx = 0; idx < inputStep.node.orderDependencies.length; idx++) {
					let dependencyStepName = inputStep.node.orderDependencies[idx];
					let dependencyStep = allSteps.find(step => step.name === dependencyStepName);
					if(dependencyStep) {
						mapStepDependencies(allSteps, dependencyStep, outputSteps);
						outputSteps.add(dependencyStep);
					}
					inputStep.batch.steps.forEach(batchDependencyStep => {
						if(batchDependencyStep.state === "Completed" && (batchDependencyStep.outcome === "Success" || batchDependencyStep.outcome === "Warnings") && batchDependencyStep.nodeIdx < inputStep.step.nodeIdx) {
							let batchStepHelper = allSteps.find(step => step.step.logId === batchDependencyStep.logId)!;
							outputSteps.add(batchStepHelper);
						}
					});
				}
			}

			backend.getBatchJobTiming(query).then(timingsDict => {
				let chartData: ChartData = { steps: {}, labelData: { "default": []} };
				let minChange = Number.MAX_VALUE;
				for(const [jobId, timing] of Object.entries(timingsDict.timings)) {
					let job = timing.jobResponse;
					minChange = Math.min(job.change!, minChange);					
					// make a map of executed labels so we can assign them to steps
					let usedLabelIndicies = [];
					for(let idx = 0; idx < job.labels!.length; idx++) {
						if(job.labels![idx].state === "Complete") {
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
					let defaultLabelItem: GraphHelperLabel = { change: job.change!, date: moment(job.createTime), steps: []  };
					let defaultLabel = job.defaultLabel;
					if(defaultLabel) {
						defaultLabel.nodes.forEach(node => {
							// check each step to see if we have timing data for it
							let step = steps.find(step => step.name === node);
							if(step) {
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
							if(step.graphRefLabel) {
								step.graphRefLabel.includedNodes.forEach(includedStepName => {
									let includedStep = steps.find(step => step.name === includedStepName)!;
									if(includedStep)
										labelSteps.add(includedStep);
								});
							}
						});
						let labelItem: GraphHelperLabel = { change: job.change!, date: moment(job.createTime), steps: Array.from(labelSteps)  };
						let chartDataKey = `${label.category}-${label.name}`;
						if(!chartData.labelData[chartDataKey])
							chartData.labelData[chartDataKey] = [];
						chartData.labelData[chartDataKey].push(labelItem);
					});
				};
				
				resolve({loading: false, chartData: chartData });
			});
		});
	}, []);

	const findTemplate = useCallback(async () => {
		if(jobDetails.stream) {
			TemplateCache.getStreamTemplates(jobDetails.stream).then(templates => {
				templates.forEach(template => {
					if(template.id === jobDetails.template?.id || template.name === jobDetails.template?.name) {
						setSelectedTemplateRef(template.ref);
						return;
					}
				});
			});
		}
	}, [jobDetails]);

	useEffect(() => {
		if(!selectedTemplateRef) {
			xRayHoveredJobStep = undefined;
			findTemplate();
		}
		else if(!cacheData) {
			const query: JobTimingsQuery = {
				streamId: jobDetails!.stream!.id,
				template: [ selectedTemplateRef.id ] 
			};	
			populateChartDataCache(query).then(value => {
				setCacheData(value)
			});
		}
	}, [findTemplate, selectedTemplateRef, cacheData, jobDetails, populateChartDataCache]);

	const classNames = mergeStyleSets({
	    lockScroll: {
	        overscrollBehavior: 'contain',
			overflow: 'auto'
	    },
		wrapper: {
			minHeight: '900px !important',
			position: 'relative',
			maxHeight: 'inherit',
		},
	});

	const selectDataPoint = (label: GraphHelperLabel | undefined, steps: GraphHelperStep[] | undefined) => {
		if(!label && !steps) {
			setXRayData(undefined);
			return;
		}
		function tryInsertIntoColumn(column: XRayColumn, batch: XRayBatch)
		{
			let idx = 0;
			for(; idx < column.batches.length; idx++)
			{
				let otherBatch = column.batches[idx];
				if(batch.reserveTime <= otherBatch.finishTime)
				{
					if(batch.finishTime < otherBatch.reserveTime)
					{
						break;
					}
					else
					{
						return false;
					}
				}
			}
	
			column.batches.splice(idx, 0, batch);
			return true;
		}
		const canvasData: XRayCanvasData = { jobStartTime: moment(), jobEndTime: moment(), minX:0, maxX: 0, minY:0, maxY: 0, canvasHeight: 0, canvasWidth: 0, columns: [], change: 0};
		//const stepStartTimes: moment.Moment[] = [];
		const stepEndTimes: moment.Moment[] = [];
		if(steps) {
			canvasData.change = steps[0].job.change!;
			//each batch should be a column. make batch -> step dictionary
			let batches: any = {};
			steps.forEach(step => {
				if(!batches[step.batch.id]) {
					batches[step.batch.id] = { batch: step.batch, steps: [] };
				}
				batches[step.batch.id].steps.push(step);

				//stepStartTimes.push(moment.utc(step.step.startTime));
				stepEndTimes.push(moment.utc(step.step.finishTime));
			});

			//const jobStartTime = moment.min(stepStartTimes);
			const jobEndTime = moment.max(stepEndTimes);

			canvasData.jobStartTime = moment.utc(steps[0].job.createTime);
			canvasData.jobEndTime = jobEndTime;
			
			
			// order the columns by groupidx, then the steps by nodeidx
			let sortedBatches = Object.entries(batches).sort(([,a], [,b]) => (a as any).batch.groupIdx - (b as any).batch.groupIdx);
			const defaultColumnWidth = 2000 / sortedBatches.length;
			sortedBatches.forEach((sortedBatch, idx) => {
				let width = Math.max(defaultColumnWidth, 210);
				width = Math.min(defaultColumnWidth, 350);

				let minX = 100 + idx * width;
				let maxX = minX + (width * (200 / 210));
				const sortedSteps: GraphHelperStep[] = Object.values((sortedBatch[1] as any).steps.sort((a: any, b: any) => a.step.nodeIdx - b.step.nodeIdx));

				let totalDependencyTime = 0;
				let waitTime = 0;
				let setupTime = 0;
				sortedSteps.forEach(step => {
					let thisStep = step;
					if(thisStep.setupTime[0] !== 0) {
						thisStep.dependencies?.forEach((dependency: any) => {
							totalDependencyTime += dependency.runTime;
							if(dependency.setupTime[0] !== 0) {
								totalDependencyTime += dependency.setupTime[0];
							}
							if(dependency.waitTime[0] !== 0) {
								totalDependencyTime += dependency.waitTime[0];
							}
						});
					}

					if(thisStep.waitTime[0] !== 0) {
						waitTime = thisStep.waitTime[0]!;
					}
					if(thisStep.setupTime[0] !== 0) {
						setupTime = thisStep.setupTime[0]!;
					}
				});

				let reserveTime = moment.utc((sortedBatch[1] as any).batch.startTime);
				let startTime = moment.utc(reserveTime).add(setupTime, 'seconds');

				let newBatch: XRayBatch = { 
					reserveTime: reserveTime,
					startTime: startTime,
					finishTime: moment.utc((sortedBatch[1] as any).batch.finishTime),
					setupTime: setupTime, 
					dependencyWaitTime: totalDependencyTime, 
					waitTime: waitTime,
					steps: sortedSteps
				};

				for(let i = 0; ;i++) {
					if(i === canvasData.columns.length) {
						canvasData.columns.push({ minX: minX, maxX: maxX, batches: [] });
					}
					if(tryInsertIntoColumn(canvasData.columns[i], newBatch)) {
						break;
					}
				}
			});
			
			canvasData.minX = 60.5;
			canvasData.minY = 5;
			canvasData.maxX = Math.max(canvasData.columns[canvasData.columns.length -1].maxX, 1800);
			canvasData.maxY = canvasData.minY + 50 + canvasData.jobEndTime.diff(canvasData.jobStartTime, 'seconds', true);

			canvasData.canvasWidth = (canvasData.maxX + canvasData.minX);
			canvasData.canvasHeight = (canvasData.maxY + canvasData.minY);
		}

		setXRayData(canvasData);
	}

	const onCanvasMouseMove = (ev: React.MouseEvent<HTMLCanvasElement, MouseEvent>) => {
		if(xRayData && canvas) {
			let rect = canvas.getBoundingClientRect();
			let scaleX = canvas.width / rect.width;
			let scaleY = canvas.height / rect.height; 

			let xPos = (ev.clientX - rect.left) * scaleX;
			let yPos = (ev.clientY - rect.top) * scaleY;

			xRayHoveredJobStep = undefined;
			for(let colIdx = 0; colIdx < xRayData.columns.length; colIdx++) {
				let column = xRayData.columns[colIdx];
				if(xPos >= column.minX && xPos <= column.maxX) {
					for(let batchIdx = 0; batchIdx < column.batches.length; batchIdx++) {
						let batch = column.batches[batchIdx];
						for(let stepIdx = 0; stepIdx < batch.steps.length; stepIdx++) {
							let step = batch.steps[stepIdx];
							let stepMinY = xRayData.minY + moment.utc(step.step.startTime).diff(xRayData.jobStartTime, 'seconds', true) + 1.0;
							let stepMaxY = xRayData.minY + moment.utc(step.step.finishTime).diff(xRayData.jobStartTime, 'seconds', true) - 1.0;
							if(yPos >= stepMinY && yPos <= stepMaxY) {
								if(xRayHoveredJobStep !== step) {
									xRayHoveredJobStep = step;
									break;
								}
							}
						}
					}
				}
			}
		}
		drawXRayChart();
	};

	const drawXRayChart = () => {
		function drawRoundedRect(context: CanvasRenderingContext2D, minX: number, minY: number, maxX: number, maxY: number, radius: number) {
			if(maxX > minX && maxY > minY)
			{
				radius = Math.min(radius, Math.min(maxY - minY, maxX - minX));

				context.beginPath();
				context.moveTo(minX + radius, minY);
				context.arc(maxX - radius, minY + radius, radius, -Math.PI * 0.5, 0.0);
				context.arc(maxX - radius, maxY - radius, radius, 0.0, +Math.PI * 0.5);
				context.arc(minX + radius, maxY - radius, radius, +Math.PI * 0.5, +Math.PI * 1.0);
				context.arc(minX + radius, minY + radius, radius, -Math.PI * 1.0, -Math.PI * 0.5);
				context.closePath();
				context.fill();
			}
		}
		function drawTimeBox(context: CanvasRenderingContext2D, minX: number, minY: number, maxX: number, maxY: number, color: string, text: string, textColor: string, underline: boolean) {
			context.fillStyle = color;
			drawRoundedRect(context, minX, minY, maxX, maxY, 5.0);

			if((maxY - minY) > 20)
			{
				context.font = "11 px sans-serif";

				let metrics: TextMetrics | undefined = undefined;
				for(;;)
				{
					metrics = context.measureText(text);
					if(metrics.width + 30 < maxX - minX)
					{
						break;
					}
					
					let idx = text.length;
					while(idx >= 0 && text.charAt(idx) !== ' ')
					{
						idx--;
					}
					if(idx === 0)
					{
						break;
					}
					text = text.substring(0, idx) + "...";
				}
				
				context.fillStyle = textColor;
				context.textAlign = 'center';
				context.textBaseline = 'middle';
				context.fillText(text, (minX + maxX) * 0.5, (minY + maxY) * 0.5);

				if(underline)
				{
					context.beginPath();
					context.strokeStyle = "white";
					context.moveTo((minX + maxX - metrics.width) * 0.5, (minY + maxY) * 0.5 + 6.0);
					context.lineTo((minX + maxX + metrics.width) * 0.5, (minY + maxY) * 0.5 + 6.0);
					context.stroke();
				}
			}
		}
		function drawAnnotation(context: CanvasRenderingContext2D, x: number, y: number, align: string, color: string, text: string, textColor: string) {
			context.font = "11 px sans-serif";
			let metrics = context.measureText(text);

			let padding = 15;
			let width = padding * 2 + metrics.width;
			let height = 20;

			let minX;
			if(align === 'right')
			{
				minX = x - width;
			}
			else if(align === 'left')
			{
				minX = x;
			}
			else
			{
				minX = x - width * 0.5;
			}

			let minY = y - height * 0.5;

			context.fillStyle = "rgba(255,255,255,0.3)";
			let border = 2.0;
			drawRoundedRect(context, minX - border, minY - border, minX + width + border, minY + height + border, 10.0 + border);

			context.fillStyle = color;
			drawRoundedRect(context, minX, minY, minX + width, minY + height, 10.0);

			context.fillStyle = textColor;
			context.textAlign = 'center';
			context.textBaseline = 'middle';
			context.fillText(text, minX + width * 0.5, minY + height * 0.5);
		}
		if(canvas && xRayData) {
			const context = canvas.getContext("2d")!;
			canvas.width = xRayData.canvasWidth;
			canvas.height = xRayData.canvasHeight;
			context.clearRect(0, 0, canvas.width, canvas.height);


			
			// draw left arrow
			context.beginPath();
			context.strokeStyle = "#FFFFFF";
			context.lineWidth = 1;

			context.moveTo(xRayData.minX - 5, xRayData.minY);
			context.lineTo(xRayData.minX + 5, xRayData.minY);
			context.moveTo(xRayData.minX, xRayData.minY);
			context.lineTo(xRayData.minX, xRayData.maxY);
			context.moveTo(xRayData.minX - 5, xRayData.maxY - 5);
			context.lineTo(xRayData.minX, xRayData.maxY);
			context.lineTo(xRayData.minX + 5, xRayData.maxY - 5);
			context.stroke();

			let hoverJobStepY = 0;
			if(xRayHoveredJobStep !== undefined) {
				let runTime = moment.utc(xRayHoveredJobStep.step.finishTime).diff(xRayData.jobStartTime, 'seconds', true);
				hoverJobStepY = xRayData.minY + runTime - 1.0;

				context.beginPath();
				context.strokeStyle = "#000000";
				context.moveTo(xRayData.minX - 5, hoverJobStepY);
				context.lineTo(xRayData.minX, hoverJobStepY);
				context.stroke();
				context.closePath();

				context.beginPath();
				context.strokeStyle = "#efefef";
				context.moveTo(xRayData.minX, hoverJobStepY);
				context.lineTo(xRayData.maxX, hoverJobStepY);
				context.stroke();
				context.closePath();

				let elapsedTime = moment.utc(runTime * 1000).format('HH:mm:ss');

				context.font = "10 px sans-serif";
				context.fillStyle = "white";
				context.textAlign = 'center';
				context.textBaseline = 'middle';
				context.fillText(elapsedTime, xRayData.minX - 8, hoverJobStepY);
			}

			// Draw interval stuff?
			for(let intervalTime = 0; ; intervalTime += 10 * 60) {
				const y = (xRayData.minY + (intervalTime)) + 0.5;
				if(y > xRayData.maxY - 20) {
					break;
				}

				context.beginPath();
				context.strokeStyle = "#e0e0e0";
				context.moveTo(xRayData.minX, y);
				context.lineTo(xRayData.maxX, y);
				context.stroke();

				if(intervalTime > 0 && (xRayHoveredJobStep !== undefined || Math.abs(y - hoverJobStepY) > 15)) {
					context.beginPath();
					context.strokeStyle = "#FFFFFF";
					context.moveTo(xRayData.minX - 5, y);
					context.lineTo(xRayData.minX, y);
					context.stroke();

					let elapsedTime = moment.utc(intervalTime * 1000).format('HH:mm:ss');

					context.font = "10 px sans-serif";
					context.fillStyle = "white";
					context.textAlign = 'right';
					context.textBaseline = 'middle';
					context.fillText(elapsedTime,xRayData.minX - 8, y);
				}
			}

			// Figure out all the dependencies of the steps we're hovering over
			let selectedDependencies: GraphHelperStep[] | undefined = undefined;
			if(xRayHoveredJobStep !== undefined) {
				selectedDependencies = [];
				selectedDependencies.push(xRayHoveredJobStep);
				for(let idx = 0; idx < selectedDependencies.length; idx++){
					let jobStep = selectedDependencies[idx];
					jobStep.dependencies?.forEach(dependency => {
						if(!selectedDependencies!.includes(dependency))
						{
							selectedDependencies!.push(dependency);
						}
					});
					
				}
			}

			const colors = [ "35AAF2", "DE6A10", "F6C000", "00882B", "C82506", "773F9B" ];

			// TODO: Hover junk
			let hoverColumn: XRayColumn | undefined = undefined;
			let hoverColor: string | undefined = undefined;

			// Draw them all
			let colorIdx = 0;
			for(let columnIdx = xRayData.columns.length - 1; columnIdx >= 0; columnIdx--) {
				const column = xRayData.columns[columnIdx];
				for(let batchIdx = column.batches.length - 1; batchIdx >= 0; batchIdx--) {

					const batch = column.batches[batchIdx];
					
					let setupMinY = xRayData.minY + batch.reserveTime.diff(xRayData.jobStartTime, 'seconds', true) + 1;//setupMaxY - batch.setupTime + 1;
					let setupMaxY = xRayData.minY + moment.utc(batch.steps[0].step.startTime).diff(xRayData.jobStartTime, 'seconds', true) - 1;
					
					drawTimeBox(context, column.minX, setupMinY, column.maxX, setupMaxY, "#ecedf1", "Setup Time", "#707070", false);
	
					let baseColor = colors[colorIdx % colors.length];
					for(let batchStep = batch.steps.length - 1; batchStep >= 0; batchStep--)
					{
						let step = batch.steps[batchStep];
						if(step === xRayHoveredJobStep) {
							hoverColumn = column;
							hoverColor = `#${baseColor}FF`;
						}
						else {
							let stepMinY = xRayData.minY + moment.utc(step.step.startTime).diff(xRayData.jobStartTime, 'seconds', true) + 1.0;
							let stepMaxY = xRayData.minY + moment.utc(step.step.finishTime).diff(xRayData.jobStartTime, 'seconds', true) - 1.0;
		
							let alpha = 'E6';
							if(selectedDependencies != null) {
								alpha = selectedDependencies.includes(step) ? 'E6' : '26';
							}
		
	
							let color = `#${baseColor}${alpha}`;
							drawTimeBox(context, column.minX, stepMinY, column.maxX, stepMaxY, color, step.name, "white", false);
						}
					}
					colorIdx++;
				}
				
			}
		
			if(xRayHoveredJobStep !== undefined) {
				let stepMinX = hoverColumn!.minX;
				let stepMaxX = hoverColumn!.maxX;
				
				let stepMinY = xRayData.minY + moment.utc(xRayHoveredJobStep.step.startTime).diff(xRayData.jobStartTime, 'seconds', true) + 1.0;
				let stepMaxY = xRayData.minY + moment.utc(xRayHoveredJobStep.step.finishTime).diff(xRayData.jobStartTime, 'seconds', true) - 1.0;
				
				let metrics = context.measureText(xRayHoveredJobStep.name);
				let labelWidth = metrics.width + 50;
				if(stepMaxX - stepMinX < labelWidth) {
					let stepMidX = (stepMinX + stepMaxX) * 0.5;
					stepMinX = stepMidX - labelWidth * 0.5;
					stepMaxX = stepMidX + labelWidth * 0.5;
				}
				
				stepMaxY = Math.max(stepMinY + 16, stepMaxY);
				context.fillStyle = 'white';
				let overlap = 3.0;
				drawRoundedRect(context, stepMinX - overlap, stepMinY - overlap, stepMaxX + overlap, stepMaxY + overlap, 5.0);

				drawTimeBox(context, stepMinX, stepMinY, stepMaxX, stepMaxY, hoverColor!, xRayHoveredJobStep!.name, "white", true);

				let stepDuration = moment(xRayHoveredJobStep.step.finishTime).diff(moment(xRayHoveredJobStep.step.startTime), 'seconds', true);
				drawAnnotation(context, stepMaxX + 5, (stepMinY + stepMaxY) * 0.5, 'left', "#e0e0e0", moment.utc(stepDuration * 1000).format("HH:mm:ss"), "black");
			}
		}
	};

	const filterChartData = (chartDataCache?: ChartDataCache) => {
		if(!chartDataCache) {
			return undefined;
		}

		function sortCalloutData(calloutData: any) {
			let sortedCallouts = Object.entries(calloutData).sort(([,a], [,b]) => (a as any).startTime.diff((b as any).startTime));
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
		if(chartDataCache && chartDataCache.loading === false) {
			let filteredChartItem = chartDataCache.chartData!;
			if(step && step.timing && step.timing.name && filteredChartItem.steps) {
				let labelChartPoints: ILineChartDataPoint[] = [];
				let stepChanges = Object.keys(filteredChartItem.steps);

				let numChanges = Math.min(numPoints, stepChanges.length);
				let idxStart = Math.max(0, stepChanges.length - numChanges * pageInfo.pageNumber);
				let idxEnd = Math.min(stepChanges.length - numChanges * (pageInfo.pageNumber - 1), stepChanges.length);
				for(let changeIdx = idxStart; changeIdx < idxEnd; changeIdx++) {
					const change = stepChanges[changeIdx];
					const helperData = filteredChartItem.steps[parseInt(stepChanges[changeIdx])];
					let dataStep = helperData.steps.find(s => s.name === step.timing!.name);
					
					if(dataStep && (dataStep.step.outcome === "Success" || dataStep.step.outcome === "Warnings") && dataStep.step.state === "Completed" ) {
						let stepRunTime = dataStep.runTime! * 1000;
						let stepWaitTime = (includeStepDependencies ? dataStep.waitTime[0]! : dataStep.waitTime[1]!) * 1000;
						let stepSetupTime = (includeStepDependencies ? dataStep.setupTime[0]! : dataStep.setupTime[1]!) * 1000;
	
						let calloutData: any = {};
						calloutData[dataStep.name] = { startTime: moment(dataStep.step.startTime!), label: msecToElapsed(stepRunTime) };
						if(includeWaitTimes) {
							stepRunTime += stepWaitTime;
							stepRunTime += stepSetupTime;
							calloutData['Wait Time'] = { startTime: moment(waitTimeDate), label: stepWaitTime };
							calloutData['Setup Time'] = { startTime: moment(setupTimeDate), label: stepSetupTime };
						}
					
						if(includeStepDependencies && dataStep.dependencies) {
							dataStep.dependencies.forEach(inputStep => {
								let inputStepRunTime = inputStep.runTime! * 1000;
								let inputWaitTime = inputStep.waitTime[0]! * 1000;
								let inputSetupTime = inputStep.setupTime[0]! * 1000;
								calloutData[inputStep.name] = { startTime: moment(inputStep.step.startTime), label: msecToElapsed(inputStepRunTime) };
								stepRunTime += inputStepRunTime;
	
								if(includeWaitTimes) {
									stepRunTime += inputWaitTime;
									stepRunTime += inputSetupTime;
									calloutData['Wait Time'].label += inputWaitTime;
									calloutData['Setup Time'].label += inputSetupTime;
								}
							
							});
						}
						if('Wait Time' in calloutData) {
							calloutData['Wait Time'].label = msecToElapsed(calloutData['Wait Time'].label);
						}

						if('Setup Time' in calloutData) {
							calloutData['Setup Time'].label = msecToElapsed(calloutData['Setup Time'].label);
						}
	
						if('Wait Time' in calloutData || includeStepDependencies) {
							calloutData['Total Time'] = { startTime: moment(totalTimeDate), label: msecToElapsed(stepRunTime) };
						}

						calloutData = sortCalloutData(calloutData);

						if(includeCost && dataStep.price) {
							calloutData['Cost'] = `$${dataStep.price.toFixed(2)}`;
							if(dataStep.jobPrice) {
								calloutData['Cost'] = `${calloutData['Cost']} (of $${dataStep.jobPrice.toFixed(2)})`;			
							} 
						}
	
	
						yMax = Math.max(yMax, stepRunTime);
						allDates.push(helperData.date);
						labelChartPoints.push({
							x: helperData.date.toDate(),
							y: stepRunTime,
							yAxisCalloutData: calloutData,
							xAxisCalloutData: `CL ${change}`,
							//onDataPointClick: () => selectDataPoint(helperData)
						});	
					} 
				}
				// add data as long as there's something relevant to display
				if(!labelChartPoints.every(point => point.y === 0)) {
					let existingProps = newChartProps.lineChartData!.find(existing => existing.legend === step.timing!.name);
					if(existingProps) {
						existingProps.data = existingProps.data.concat(labelChartPoints);
					}
					else {
						newChartProps.lineChartData!.push({
							color: colorSteps[colorStep],
							data: labelChartPoints,
							legend: step.timing.name
						});
						colorStep++;
						if(colorStep === colorSteps.length) {
							colorStep = 0;
						}
					}
				}
			}
			else {
				let labels = Object.entries(chartDataCache.chartData!.labelData);
				if(label) {
					let key = `${label.category}-${label.name}`;
					if(key === "Other-Other") {
						key = "default";
					}
					labels = [ [ key, chartDataCache.chartData!.labelData[key] ] ];
				}

				const changesToAllStepsMap: { [change: number]: Set<GraphHelperStep> } = {};
				for(const [labelChange, labelChanges] of labels) {
					if(labelChange !== "undefined-undefined" && labelChanges) {
						for(let changeIdx = 0; changeIdx < labelChanges.length; changeIdx++) {
							let changeLabel = labelChanges[changeIdx];
							if(!changesToAllStepsMap[changeLabel.change]) {
								changesToAllStepsMap[changeLabel.change] = new Set<GraphHelperStep>();
							}
							let change = labelChanges[changeIdx];
							let steps = change.steps;
							if(steps.length) {
								if(steps.every(step => (step.step.outcome === "Success" || step.step.outcome === "Warnings") && step.step.state === "Completed" )) {
									steps.forEach(step => {
										changesToAllStepsMap[changeLabel.change].add(step);
									});
								}
							}
						}
					}
				}

				for(const [labelChange, labelChanges] of labels) {
					if(labelChange !== "undefined-undefined") {
						let labelChartPoints: ILineChartDataPoint[] = [];

						if (!labelChanges?.length) {
							continue;
						}

						let numChanges = Math.min(numPoints, labelChanges.length);
						let idxStart = numChanges * (pageInfo.pageNumber - 1);
						let idxEnd = Math.min(numChanges * pageInfo.pageNumber, labelChanges.length);
						for(let changeIdx = idxStart; changeIdx < idxEnd; changeIdx++) {
							let change = labelChanges[changeIdx];
							let steps = change.steps;
							if(steps.length) {
								if(steps.every(step => (step.step.outcome === "Success" || step.step.outcome === "Warnings") && step.step.state === "Completed" )) {
									// get the overall
									let labelRunTime = 0;
									let calloutData: any = {};
									let totalPrice = 0;
									let jobPrice: number = NaN;
									// if on the jobdetails view (so no label), just get aggregate label times for the graph lines
									if(!label) {
										labelRunTime = steps.map(step => { return step.runTime! }).reduce((a, b) => a + b, 0) * 1000;
										calloutData[labelChange] = { startTime: moment(0), label: msecToElapsed(labelRunTime) };

										if(includeWaitTimes || includeCost) {
											if(includeWaitTimes) {
												calloutData['Total Time'] = { startTime: moment(totalTimeDate), label: labelRunTime };
												calloutData['Wait Time'] = { startTime: moment(waitTimeDate), label: 0 };	
												calloutData['Setup Time'] = { startTime: moment(setupTimeDate), label: 0 };
											}
											steps.forEach(step => {
												if(includeWaitTimes) {
													let waitTime = step.waitTime[0]! * 1000;
													let setupTime = step.setupTime[0]! * 1000;
													labelRunTime += waitTime;	
													labelRunTime += setupTime;
													calloutData['Wait Time'].label += waitTime;
													calloutData['Total Time'].label += waitTime;

													calloutData['Setup Time'].label += setupTime;
													calloutData['Total Time'].label += setupTime;
												}
												if(includeCost && step.price) {
													totalPrice += step.price;
													if(step.jobPrice) {
														// these should all be the same?
														jobPrice = step.jobPrice;
													}
												}
											});
										}
									
									}
									// otherwise get the individual steps
									else {
										calloutData['Total Time'] = { startTime: moment(totalTimeDate), label: 0 };
										if(includeWaitTimes) {
											calloutData['Wait Time'] = { startTime: moment(waitTimeDate), label: 0 };
											calloutData['Setup Time'] = { startTime: moment(setupTimeDate), label: 0 };
										}
										steps.forEach(step => {
											let runTime = step.runTime! * 1000;
											calloutData[step.name] = { startTime: moment(step.step.startTime!), label: msecToElapsed(runTime) };

											if(includeWaitTimes) {
												let waitTime = step.waitTime[0]! * 1000;
												let setupTime = step.setupTime[0]! * 1000;
												runTime += waitTime;
												runTime += setupTime;
												calloutData['Wait Time'].label += waitTime;
												calloutData['Setup Time'].label += setupTime;
											}
											if(includeCost && step.price) {
												totalPrice += step.price;
												if(step.jobPrice) {
													jobPrice = step.jobPrice;
												}
											}
											calloutData['Total Time'].label += runTime;
											labelRunTime += runTime;
											
										});
									}

									yMax = Math.max(yMax, labelRunTime);

									if('Wait Time' in calloutData) {
										calloutData['Wait Time'].label = msecToElapsed(calloutData['Wait Time'].label);
									}
									if('Setup Time' in calloutData) {
										calloutData['Setup Time'].label = msecToElapsed(calloutData['Setup Time'].label);
									}
									if('Total Time' in calloutData) {
										calloutData['Total Time'].label = msecToElapsed(calloutData['Total Time'].label);
									}

									calloutData = sortCalloutData(calloutData);

									// always do cost last					
									if(includeCost) {
										let costString = `$${totalPrice.toFixed(2)}`;
										if(!isNaN(jobPrice)) {
											costString = `${costString} (of $${jobPrice.toFixed(2)})`;
										}
										calloutData['Cost'] = costString;
									}
	

									allDates.push(change.date);
									labelChartPoints.push({
										x: change.date.toDate(),
										y: labelRunTime,
										yAxisCalloutData: calloutData,
										xAxisCalloutData: `CL ${change.change}`,
										onDataPointClick: () => selectDataPoint(change, Array.from(changesToAllStepsMap[change.change]))
									});	
								}
							}
						}
						// add data as long as there's something relevant to display
						if(!labelChartPoints.every(point => point.y === 0)) {
							let existingProps = newChartProps.lineChartData!.find(existing => existing.legend === labelChange);
							if(existingProps) {
								existingProps.data = existingProps.data.concat(labelChartPoints);
							}
							else {
								newChartProps.lineChartData!.push({
									color: colorSteps[colorStep],
									data: labelChartPoints,
									legend: labelChange
								});
								colorStep++;
								if(colorStep === colorSteps.length) {
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
			if(yMaxValue > 122400000) {
				leftMargin = 80;
			}
			else if(yMaxValue > 36000000) {
				leftMargin = 60;
			}
			else if(yMaxValue > 86400000) {
				leftMargin = 70;
			}
			else if(yMaxValue > 3600000) {
				leftMargin = 50;
			}
			
			filteredChartData.leftMargin = Math.max(filteredChartData.leftMargin ?? 0, leftMargin);
			filteredChartData.yMaxValue = Math.max(filteredChartData.yMaxValue ?? 0, yMaxValue);

			// filter out default label if applicable
			if(newChartProps.lineChartData) {
				// do something (convert to steps?) if we only have a default label
				if(newChartProps.lineChartData.length === 1 && newChartProps.lineChartData[0].legend === "default") {

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

	let filteredChartState: FilteredChartData | undefined = filterChartData(cacheData);
	
	if(canvas && xRayData) {
		drawXRayChart();
	}

	let placeholderDiv = <div></div>;
	let titleText = 'Trends';
	if(filteredChartState && filteredChartState?.chartProps) {
		if(filteredChartState.chartProps?.lineChartData?.length) {
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
								if(!props || (!props[0] && !props[1])) {
									return null;
								}

								let hoverPoint1 = props[0];
								let hoverPoint2 = props[1];

								let dataPoint1 = hoverPoint1 ? hoverPoint1.values.find((point: { legend: string | number; y: string | number }) => point.y === hoverPoint1!.hoverYValue) : undefined;
								let dataPoint2 = hoverPoint2 ? hoverPoint2.values.find((point: { legend: string | number; y: string | number }) => point.y === hoverPoint2!.hoverYValue) : undefined;

								if(hoverPoint1 && dataPoint1) {
									return <HChartHoverCard props1={hoverPoint1} dataPoint1={dataPoint1} props2={hoverPoint2} dataPoint2={dataPoint2}/>
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
		if(direction > 0 && newPageInfo.zoomLevel < 10){
			newPageInfo.zoomLevel++;
			newPageInfo.pageNumber = Math.max(1, newPageInfo.pageNumber - 1);
		}
		else if(direction < 0 && newPageInfo.zoomLevel > 1) {			
			newPageInfo.zoomLevel--;
		}
		setPageInfo(newPageInfo);
	};

	const onPageChange = (direction: number) => {
		let newPageInfo = Object.assign({}, pageInfo);
		newPageInfo.pageNumber += direction;
		let maxPage = Math.ceil(newPageInfo.pageSize / newPageInfo.zoomLevel);
		console.log(`maxPage: ${maxPage}`);
		if(newPageInfo.pageNumber < 1){
			newPageInfo.pageNumber = 1;
		}
		else if(newPageInfo.pageNumber > maxPage) {
			newPageInfo.pageNumber = maxPage;
		}
		setPageInfo(newPageInfo);

	};

	const onChartWheelEnter = (e: any) => { 
		document.addEventListener('wheel', preventChartScroll, { passive: false } ) 
	};

	const onChartWheelLeave = (e: any) => { 
		document.removeEventListener('wheel', preventChartScroll, false ) 
	};

	let spinnerObj: any = <Spinner styles={{ root: { display: 'inline-block', position: 'relative', top: -10 }}} size={SpinnerSize.large}/>
	let statusText: string = "Fetching template data...";

	if(selectedTemplateRef) {
		statusText =  `Fetching trends data...`;
	}

	const nextIcon: IIconProps = { iconName: 'Forward' };
	const backIcon: IIconProps = { iconName: 'Back' };

	let statusDiv = <div>{ spinnerObj } <Text styles={{ root: { position: 'relative', top: -18, left: 5 }}}> { statusText } </Text></div>;
	if(cacheData && filteredChartState) {
		let totalValues = Object.values(cacheData.chartData!.labelData).map(c => c.length).reduce((a, b) => Math.max(a, b));
		let minShown = ((pageInfo.pageNumber - 1) * pageInfo.pageSize * pageInfo.zoomLevel) + 1;
		let maxShown = Math.min(totalValues, Math.min(totalValues, pageInfo.pageNumber * pageInfo.pageSize) * pageInfo.zoomLevel);
		
		statusText = `Showing ${minShown}-${maxShown} of ${totalValues}`;
		
		statusDiv = (<div>
						<IconButton styles={{ root: { position: 'relative', top: -18, left: -4 }}} iconProps={backIcon} onClick={() => { onPageChange(1); }}/>
						<IconButton styles={{ root: { position: 'relative', top: -18, left: -4 }}} iconProps={nextIcon} onClick={() => { onPageChange(-1) }}/>
						<Text styles={{ root: { position: 'relative', top: -23, left: 0 }}}> { statusText } </Text>
					</div>);
	}
	
	return (<Stack styles={{ root: { paddingTop: 18, paddingRight: 12 } }}>
		<Stack className={hordeClasses.raised}>
			<Stack>
				<Stack horizontal horizontalAlign="space-between" styles={{ root: { minHeight: 32 }}}>
					<Text variant="mediumPlus" styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>{titleText}</Text>
					{step && !xRayData && <Toggle label="Include dependencies" checked={includeStepDependencies} onText="On" offText="Off" onChange={() => setIncludeStepDependencies(!includeStepDependencies)} />}
					{ !xRayData && <Toggle label="Include wait times" checked={includeWaitTimes} onText="On" offText="Off" onChange={() => setIncludeWaitTimes(!includeWaitTimes)}/> }
					{/* { !xRayData && <Toggle label="Include cost" checked={includeCost} onText="On" offText="Off" onChange={() => setIncludeCost(!includeCost)}/> } */}
				</Stack>
				<Stack styles={{ root: { position: 'relative', top: -10 }}}>
					{ statusDiv }
				</Stack>
				<Stack styles={{ root: { width: 1268, minHeight: 350, height: 350 }}} onWheel={onChartScroll} onMouseEnter={onChartWheelEnter} onMouseLeave={onChartWheelLeave}>
					{ placeholderDiv }
					<Dialog
						modalProps={{
							isBlocking: false,
							dragOptions: {
							closeMenuItemText: "Close",
							moveMenuItemText: "Move",
							menu: ContextualMenu
							},
							styles: {
							root: {
								selectors: {
									".ms-Dialog-title": {
										paddingTop: '24px',
										paddingLeft: '32px'
									}
								}
							}
							}
						}}
						onDismiss={() => { selectDataPoint(undefined, undefined); xRayHoveredJobStep = undefined; }}
						//className={historyStyles.dialog}
						hidden={xRayData === undefined}
						minWidth={1920}
						dialogContentProps={{
							type: DialogType.close,
							onDismiss: () => { selectDataPoint(undefined, undefined); xRayHoveredJobStep = undefined; },
							title: `X-Ray - CL ${ xRayData ? xRayData.change : 0} `,
						}}
					>
						<canvas ref={newRef => setCanvas(newRef)} width={1920} height={0} onMouseMove={(ev) => onCanvasMouseMove(ev)}>
						</canvas>
					</Dialog>
				</Stack>
				
			</Stack>

		</Stack></Stack>);
};
