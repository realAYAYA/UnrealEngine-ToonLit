import { DetailsList, DetailsListLayoutMode, Dropdown, IColumn, IconButton, IDropdownOption, mergeStyleSets, Modal, SelectionMode, Spinner, SpinnerSize, Stack, Text, TextField } from "@fluentui/react";
import * as d3 from "d3";
import { action, makeObservable, observable } from 'mobx';
import { observer } from 'mobx-react-lite';
import React, { useState } from "react";
import backend from "../backend";
import { GetSuiteTestDataResponse, GetTestDataDetailsResponse, GetTestDataRefResponse, GetTestMetaResponse, GetTestResponse, GetTestSuiteResponse, TestOutcome } from "../backend/Api";
import dashboard, { StatusColor } from "../backend/Dashboard";
import { projectStore } from "../backend/ProjectStore";
import { useWindowSize } from "../base/utilities/hooks";
import { getHumanTime, getShortNiceTime } from "../base/utilities/timeUtils";
import { hordeClasses, modeColors } from "../styles/Styles";
import { AutomationSuiteTest } from "./AutomationSuiteTest";

type TestId = string;
type SelectionType = d3.Selection<SVGGElement, unknown, null, undefined>;
type DivSelectionType = d3.Selection<HTMLDivElement, unknown, null, undefined>;
type TestState = "Success" | "Failed" | "Consecutive Failures" | "Skipped" | "Unspecified";
type FilterState = {
   testState?: TestState;
   text?: string;
}

const suiteGraphWidth = 1180;

type TestStatus = {
   testId: string,
   success?: number;
   error?: number;
   skip?: number;
   unspecified?: number;
}

type TestSelection = {
   ref: GetTestDataRefResponse;
   testId: string;
   details: GetTestDataDetailsResponse;
}

type TestMetrics = {
   total: number;
   error: number;
   success: number;
   skipped: number;
   unspecified: number;
}

class SuiteHandler {

   constructor(suite: GetTestSuiteResponse, suiteRefs: GetTestDataRefResponse[], metaData: GetTestMetaResponse) {

      makeObservable(this);

      this.suite = suite;

      const dedupe = new Set<number>();

      this.suiteRefs = suiteRefs.filter(r => {
         if (!dedupe.has(r.buildChangeList)) {
            dedupe.add(r.buildChangeList);
            return true;
         }
         return false;
      });

      if (this.suiteRefs.length !== suiteRefs.length) {
         console.warn("Warning: Test suite details had duplicate ref changelists");
      }

      this.metaData = metaData;

      this.metaName = `${metaData.platforms.join(" - ")} / ${metaData.configurations.join(" - ")} / ${metaData.buildTargets.join(" - ")} / ${metaData.rhi === "default" ? "Default" : metaData.rhi?.toUpperCase()} / ${metaData.variation === "default" ? "Default" : metaData.variation?.toUpperCase()}`;
         
      this.load();
   }

   setSelection(selection?: TestSelection) {

      if ((this.selection?.ref.id === selection?.ref.id) && (this.selection?.testId === selection?.testId)) {
         return;
      }

      this.selection = selection;
      this.setSelectionUpdated();
   }

   @action
   setUpdated() {
      this.updated++;
   }

   @action
   setSelectionUpdated() {
      this.selectionUpdated++;
   }

   async load() {

      console.log(`Loading ${this.suiteRefs.length} suite ref details for metaData: ${JSON.stringify(this.metaData)}`);

      this.changeDates = new Map();
      this.suiteRefs.forEach(r => {

         this.refs.set(r.id, r);

         if (!this.refDates.get(r.id)) {
            const timestamp = r.id.substring(0, 8)
            this.refDates.set(r.id, new Date(parseInt(timestamp, 16) * 1000));
         }

         const date = this.changeDates.get(r.buildChangeList);
         const rdate = this.refDates.get(r.id)!;
         if (!date || date.getTime() > rdate.getTime()) {
            this.changeDates.set(r.buildChangeList, rdate);
         }
      });


      let details = await backend.getTestDetails(this.suiteRefs.map(r => r.id));
      details = details.filter(d => !!d.suiteTests);
      details.forEach(d => this.testDetails.set(d.id, d));

      const uniqueTests = new Set<string>(details.map(d => d.suiteTests!.map(t => t.testId)).flat())
      const request = Array.from(uniqueTests).filter(tid => !SuiteHandler.tests.has(tid));

      if (request.length) {
         const tests = await backend.getTests(Array.from(uniqueTests).filter(tid => !SuiteHandler.tests.has(tid)));

         const replace = ["Project.Functional Tests.Tests.", "Project.Maps.AllInPIE."];

         function fixName(name: string) {

            for (let r of replace) {
               if (name.startsWith(r)) {
                  name = name.substring(r.length)
               }
            }
            return name;
         }

         tests.forEach(t => {
            SuiteHandler.tests.set(t.id, t);
            const fixed = fixName(t.displayName ?? t.name);
            SuiteHandler.fixedTestNames.set(t.id, fixed);
            SuiteHandler.lowerCaseTestNames.set(t.id, fixed.toLowerCase());
         });
      }

      const refs = this.suiteRefs.sort((a, b) => a.buildChangeList - b.buildChangeList).reverse();

      for (const ref of refs) {
         const details = this.testDetails.get(ref.id);
         const suiteTests = details?.suiteTests;
         if (!suiteTests) {
            return;
         }

         for (const test of suiteTests) {

            let testData = this.testData.get(test.testId);

            if (!testData) {
               testData = [];
               this.testData.set(test.testId, testData);
            }

            testData.push({ ...test, refId: ref.id });

            let status = this.testStatus.get(test.testId);
            if (!status) {
               status = { testId: test.testId };
               this.testStatus.set(test.testId, status);
            }
         }
      }

      for (const [, testData] of this.testData) {


         for (const test of testData) {

            const error = test.outcome === "Failure";
            if (!error) {
               break;
            }
            const status = this.testStatus.get(test.testId)!;
            status.error = status.error ? status.error + 1 : 1;
         }

         for (const test of testData) {

            const skipped = test.outcome === "Skipped";
            if (!skipped) {
               break;
            }
            const status = this.testStatus.get(test.testId)!;
            status.skip = status.skip ? status.skip + 1 : 1;
         }

         for (const test of testData) {

            const unspecified = test.outcome === "Unspecified";
            if (!unspecified) {
               break;
            }
            const status = this.testStatus.get(test.testId)!;
            status.unspecified = status.unspecified ? status.unspecified + 1 : 1;
         }


         for (const test of testData) {

            const success = (test.outcome !== "Failure" && test.outcome !== "Skipped" && test.outcome !== "Unspecified");
            if (!success) {
               break;
            }
            const status = this.testStatus.get(test.testId)!;
            status.success = status.success ? status.success + 1 : 1;
         }

      }

      const metrics = this.metrics = {
         total: 0,
         error: 0,
         success: 0,
         skipped: 0,
         unspecified: 0
      };

      this.testStatus.forEach((v, k) => {
         metrics.total++;
         if (v.error) {
            metrics.error++;
         }
         if (v.success) {
            metrics.success++;
         }
         if (v.skip) {
            metrics.skipped++;
         }
         if (v.unspecified) {
            metrics.unspecified++;
         }
      });

      this.loaded = true;

      this.setUpdated();

      // const sdetails = this.testDetails.get(debugRef)!;
      //this.setSelection({ ref: this.suiteRefs.find(s => s.id === debugRef)!, details: sdetails, testId: debugTestId })
   }

   setFilterState(nstate?: FilterState) {

      if ((this.filterState?.testState === nstate?.testState) && (this.filterState?.text === nstate?.text)) {
         return;
      }

      this.filterState = (nstate?.testState || nstate?.text) ? nstate : undefined;

      this.setUpdated();
   }

   getFilterState(): FilterState | undefined {
      return this.filterState;
   }


   getFilteredTestData(testId?: string): TestData[] {

      if (!this.filterState) {
         if (testId) {
            return this.testData.get(testId)?.flat() ?? [];
         }
         return Array.from(this.testData.values()).flat();
      }

      const testData: TestData[] = [];

      const textLowercase = this.filterState.text?.toLowerCase();

      let testStatus = this.testStatus;

      if (testId) {
         const ts = this.testStatus.get(testId);
         testStatus = new Map();
         if (ts) {
            testStatus.set(testId, ts);
         }
      }

      testStatus.forEach(ts => {

         const sdata = this.testData.get(ts.testId);
         if (!sdata) {
            return;
         }

         const test = SuiteHandler.tests.get(ts.testId);

         if (!test) {
            return;
         }

         if (textLowercase) {
            const name = SuiteHandler.lowerCaseTestNames.get(test.id)!;
            if (name.indexOf(textLowercase) === -1) {
               return;
            }
         }

         if (!this.filterState!.testState) {
            testData.push(...sdata);
            return;
         }

         const testState = this.filterState!.testState;

         if (testState === "Failed") {
            if (ts.error) {
               testData.push(...sdata);
            }
         }

         if (testState === "Consecutive Failures") {
            if (ts.error && ts.error > 1) {
               testData.push(...sdata);
            }
         }

         if (testState === "Success") {
            if (ts.success) {
               testData.push(...sdata);
            }
         }

         if (testState === "Skipped") {
            if (ts.skip) {
               testData.push(...sdata);
            }
         }

         if (testState === "Unspecified") {
            if (ts.unspecified) {
               testData.push(...sdata);
            }
         }

      });

      return testData;

   }

   @observable
   updated: number = 0;

   @observable
   selectionUpdated: number = 0;

   loaded = false

   testDetails = new Map<string, GetTestDataDetailsResponse>();
   suite: GetTestSuiteResponse;
   suiteRefs: GetTestDataRefResponse[];

   refs = new Map<string, GetTestDataRefResponse>();

   metaData: GetTestMetaResponse;
   metaName: string;

   // quantized so common CL -> lands at same time in views
   changeDates = new Map<number, Date>();
   refDates = new Map<string, Date>();


   static tests = new Map<TestId, GetTestResponse>();

   private filterState?: FilterState;

   testStatus: Map<TestId, TestStatus> = new Map();
   private testData: Map<TestId, TestData[]> = new Map();

   selection?: TestSelection;

   metrics?: TestMetrics;

   static fixedTestNames = new Map<TestId, string>();
   static lowerCaseTestNames = new Map<TestId, string>();

}

type TestData = GetSuiteTestDataResponse & {
   refId: string;
}

class SuiteGraphRenderer {

   constructor(handler: SuiteHandler, testId?: string) {
      this.testId = testId;
      this.handler = handler;
      this.margin = { top: 0, right: 8, bottom: 0, left: 16 };

      this.headerOnly = !testId;

   }

   initData() {

      const handler = this.handler;

      this.testData = this.headerOnly ? [] : handler.getFilteredTestData(this.testId);
   }

   render(container: HTMLDivElement) {

      const handler = this.handler;

      if (!handler.testDetails.size || (this.hasRendered && !this.forceRender)) {
         return;
      }

      this.clear();

      this.initData();

      this.hasRendered = true;
      this.forceRender = false;

      const X = d3.map(this.testData, (t) => t);
      const xDomain = d3.extent(handler.changeDates.values(), d => d.getTime() / 1000);

      let Y: TestOutcome[] = [];
      const scolors = dashboard.getStatusColors();
      const colors: Record<string, string> = {
         "Success": scolors.get(StatusColor.Success)!,
         "Failure": scolors.get(StatusColor.Failure)!,
         "Warning": scolors.get(StatusColor.Warnings)!,
         "Unspecified": scolors.get(StatusColor.Unspecified)!,
         "Skipped": scolors.get(StatusColor.Skipped)!
      };

      Y = d3.map(this.testData, (d) => d.outcome);

      const width = suiteGraphWidth - 32; // important: the 32 offset is to provide more mouse room to roll down latest CL's
      const height = this.headerOnly ? 20 : 16;

      const xRange = [this.margin.left, width - this.margin.right];

      const xScale = d3.scaleTime(xDomain as any, xRange);

      const I = d3.range(X.length);

      const svgId = `suite_svg_${id_counter++}`;

      const svg = d3.select(container)
         .append("svg")
         .attr("id", svgId)
         .attr("width", width + 48)
         .attr("height", height)
         .attr("viewBox", [0, 0, width, height] as any)


      const g = svg.append("g")

      if (!this.headerOnly) {
         g.append("line")
            .attr("stroke", () => {
               return dashboard.darktheme ? "#6D6C6B" : "#4D4C4B";
            })
            .attr("stroke-width", 1)
            .attr("stroke-linecap", 4)
            .attr("stroke-opacity", dashboard.darktheme ? 0.35 : 0.25)
            .attr("x1", () => this.margin.left)
            .attr("x2", () => width - this.margin.right)
            .attr("y1", () => 8)
            .attr("y2", () => 8)
      }

      const radius = 3.5
      g.selectAll("circle")
         .data(I)
         .join("circle")
         .attr("id", i => `suite_rect${this.testData[i as any].refId}`)
         .attr("cx", i => {
            const ref = handler.refs.get(X[i as any].refId)!;
            return xScale(handler.changeDates.get(ref.buildChangeList)!.getTime() / 1000);
         })
         .attr("cy", i => 8)
         .attr("fill", i => colors[Y[i as any]] ?? "#0000FF")
         .attr("width", 4)
         .attr("r", radius);

      const xAxis = (g: SelectionType) => {

         const dateMin = new Date(xDomain[0]! * 1000);
         const dateMax = new Date(xDomain[1]! * 1000);

         let ticks: number[] = [];
         for (const date of d3.timeDays(dateMin, dateMax, 1).reverse()) {
            ticks.push(date.getTime() / 1000);
         }

         if (ticks.length > 14) {
            let nticks = [...ticks];
            // remove first and last, will be readded 
            const first = nticks.shift()!;
            const last = nticks.pop()!;

            const n = Math.floor(nticks.length / 12);

            const rticks: number[] = [];
            for (let i = 0; i < nticks.length; i = i + n) {
               rticks.push(nticks[i]);
            }

            rticks.unshift(first);
            rticks.push(last);
            ticks = rticks;

         }

         g.attr("transform", `translate(0,${this.headerOnly ? 16 : 0})`)
            .style("font-family", "Horde Open Sans Regular")
            .style("font-size", "9px")
            .call(d3.axisTop(xScale)
               .tickValues(ticks)
               .tickFormat(d => {
                  return this.headerOnly ? getHumanTime(new Date((d as number) * 1000)) : "";
               })
               .tickSizeOuter(0))
            .call(g => g.select(".domain").remove())
            .call(g => g.selectAll(".tick line").attr("stroke-opacity", dashboard.darktheme ? 0.35 : 0.25).clone()
               .attr("y2", height - this.margin.bottom)
            )
      }

      svg.append("g").call(xAxis, xScale)

      const root = d3.select("#root_suite_test_list") as any as DivSelectionType

      const tooltip = this.tooltip = root
         .append("div")
         .attr("id", "tooltip")
         .style("display", "none")
         .style("background-color", modeColors.background)
         .style("border", "solid")
         .style("border-width", "1px")
         .style("border-radius", "3px")
         .style("border-color", dashboard.darktheme ? "#413F3D" : "#2D3F5F")
         .style("padding", "8px")
         .style("position", "absolute")
         .style("z-index", "1")
         .style("pointer-events", "none");

      svg.on("mousemove", (event) => handleMouseMove(event))
      svg.on("mouseleave", (event) => handleMouseLeave(event))
      svg.on("click", (event) => handleMouseClick(event))

      const closestRef = (x: number, y: number): GetTestDataRefResponse | undefined => {

         let closest = this.testData.reduce((best, data, i) => {
            const ref = handler.refs.get(data.refId)!;
            const timeStamp = handler.changeDates.get(ref.buildChangeList)!.getTime() / 1000;
            const sx = xScale(timeStamp);
            let absx = Math.abs(sx - x)

            if (absx < best.value) {
               return { index: i, value: absx };
            }
            else {
               return best;
            }
         }, { index: 0, value: Number.MAX_SAFE_INTEGER });

         if (closest) {
            return handler.refs.get(this.testData[closest.index]?.refId);
         }

         return undefined;

      }

      const handleMouseMove = (event: any) => {

         const mouseX = d3.pointer(event)[0];
         const mouseY = d3.pointer(event)[1];

         const closest = closestRef(mouseX, mouseY);
         if (closest) {
            svg.selectAll(`circle`).attr("r", radius);
            svg.select(`#suite_rect${closest.id}`).attr("r", radius * 2);

            const timestamp = closest.id.substring(0, 8)

            const date = getShortNiceTime(new Date(parseInt(timestamp, 16) * 1000), true, true);
            let desc = `CL ${closest.buildChangeList} <br/>`;
            //desc += `Duration ${msecToElapsed(moment.duration(closest.duration).asMilliseconds(), true, true)} <br/>`;
            desc += `${date} <br/>`;

            const timeStamp = handler.changeDates.get(closest.buildChangeList)!.getTime() / 1000;
            const tx = xScale(timeStamp);

            updateTooltip(true, tx, 0, desc);
         }
      }

      const handleMouseLeave = (event: any) => {
         svg.selectAll(`circle`).attr("r", radius);
         tooltip.style("display", "none");
      }

      const handleMouseClick = (event: any) => {

         const mouseX = d3.pointer(event)[0];
         const mouseY = d3.pointer(event)[1];

         const closest = closestRef(mouseX, mouseY);
         if (closest && this.testId) {
            const details = handler.testDetails.get(closest.id);
            if (details) {
               handler.setSelection({ ref: closest, details: details, testId: this.testId });
            }
         }
      }

      const updateTooltip = (show: boolean, x?: number, y?: number, html?: string) => {

         if (!tooltip) {
            return;
         }

         x = x ?? 0;
         y = y ?? 0;

         var elem = document.querySelector('#' + svgId) as any;
         var parent = document.querySelector('#root_suite_test_list') as any;

         if (!elem || !parent) {
            return;
         }

         const b = elem.getBoundingClientRect()
         const pb = parent.getBoundingClientRect()

         x += (b.left - pb.left);
         y += (b.top - pb.top);

         tooltip
            .style("display", "block")
            .html(html ?? "")
            .style("position", `absolute`)
            .style("width", `max-content`)
            .style("top", (y - 52) + "px")
            .style("left", `${x + 24}px`)
            .style("transform", "translateX(-108%)")
            .style("font-family", "Horde Open Sans Semibold")
            .style("font-size", "10px")
            .style("line-height", "16px")
            .style("shapeRendering", "crispEdges")
            .style("stroke", "none")
      }

   }

   clear() {

   }

   tooltip?: DivSelectionType;

   testId?: string;
   testData: TestData[] = [];
   headerOnly = false;

   handler: SuiteHandler;

   private readonly clipId = "suite_details_clip_path";

   margin: { top: number, right: number, bottom: number, left: number }
   hasRendered = false;
   forceRender = false;
}

const styles = mergeStyleSets({
   list: {
      selectors: {
         '.ms-List-cell,.ms-DetailsRow-cell,.ms-DetailsRow': {
            paddingTop: 0,
            paddingBottom: 0,
            paddingRight: 0,
            paddingLeft: 4,
            minHeight: 0,
            height: 20
         },
         '.ms-List-cell:nth-child(odd) .ms-DetailsRow': {
            background: "#DDDFDF",
         },
         '.ms-List-cell:nth-child(even) .ms-DetailsRow': {
            backgroundColor: "#FFFFFF",
         }

      }
   }
});



const SuiteGraph: React.FC<{ handler: SuiteHandler, testId?: string }> = observer(({ handler, testId }) => {

   const graph_container_id = `automation_suite_graph_container_${id_counter++}`;

   const [container, setContainer] = useState<HTMLDivElement | null>(null);
   const [state, setState] = useState<{ graph?: SuiteGraphRenderer }>({});

   // subscribe
   if (handler.updated) { }

   if (!state.graph) {
      setState({ ...state, graph: new SuiteGraphRenderer(handler, testId) })
      return null;
   }

   if (container) {
      try {
         state.graph?.render(container);
      } catch (err) {
         console.error(err);
      }

   }

   return <Stack className={hordeClasses.horde}>
      <Stack style={{ paddingLeft: 8, paddingTop: 8 }}>
         <div id={graph_container_id} className="horde-no-darktheme" style={{ shapeRendering: "crispEdges", userSelect: "none", position: "relative" }} ref={(ref: HTMLDivElement) => setContainer(ref)} onMouseEnter={() => { }} onMouseLeave={() => { }} />
      </Stack>
   </Stack>;

})

const SuiteOperationsBar: React.FC<{ handler: SuiteHandler }> = observer(({ handler }) => {

   if (handler.updated) { }

   const stateItems: IDropdownOption[] = ["All", "Success", "Failed", "Consecutive Failures", "Skipped", "Unspecified"].map(state => {
      return {
         key: state,
         text: state
      };
   });


   return <Stack className={hordeClasses.modal} horizontal style={{ paddingBottom: 18 }}>
      <Stack className="horde-no-darktheme" grow />
      <Stack horizontal tokens={{ childrenGap: 24 }} verticalFill={true} verticalAlign={"center"}>
         <Stack>
            <Dropdown style={{ width: 180 }} options={stateItems} selectedKey={handler.getFilterState()?.testState ?? "All"}
               onChange={(event, option, index) => {

                  let cstate = handler.getFilterState() ?? {};
                  if (option) {
                     if (option.key === "All") {
                        if (cstate.testState) {
                           handler.setFilterState({ ...cstate, testState: undefined })
                        }
                     } else {
                        if (!cstate) {
                           cstate = {};
                        }
                        handler.setFilterState({ ...cstate, testState: option.key as TestState });
                     }
                  }
               }} />
         </Stack>
         <Stack>
            <TextField placeholder="Filter" style={{ width: 190 }} spellCheck={false} autoComplete="off" deferredValidationTime={1000} validateOnLoad={false} onGetErrorMessage={(newValue: any) => {
               let cstate = handler.getFilterState() ?? {};
               if (!newValue) {
                  newValue = undefined;
               }
               if (cstate.text !== newValue) {
                  handler.setFilterState({ ...cstate, text: newValue });
               }
               return undefined
            }} />
         </Stack>

      </Stack>
   </Stack>;
})

let id_counter = 0;

const SuiteTestView: React.FC<{ handler: SuiteHandler }> = observer(({ handler }) => {

   if (handler.selectionUpdated) { }

   const selection = handler.selection;

   if (selection == null) {
      return null;
   }

   const test = SuiteHandler.tests.get(selection.testId);
   if (!test || !handler.metaData || !handler.suite) {
      return null;
   }

   return <Stack key={`suite_test_view_${id_counter++}`} style={{ flexShrink: 0, height: "100%" }}>
      <AutomationSuiteTest suite={handler.suite} test={test} testRef={selection.ref} details={selection.details} metaData={handler.metaData} onClose={() => { }} />
   </Stack>

});

const SuiteTestList: React.FC<{ handler: SuiteHandler }> = observer(({ handler }) => {

   const windowSize = useWindowSize();

   if (handler.updated) { }

   interface TestItem {
      name: string;
      testId: string;
   };

   const scolors = dashboard.getStatusColors();


   const columns = [
      { key: 'name_column', name: 'Name', minWidth: 320, maxWidth: 320, isResizable: false },
      { key: 'graph_column', name: 'Graph', minWidth: suiteGraphWidth + 8, maxWidth: suiteGraphWidth + 8, isResizable: false },
   ];

   const renderItem = (item: TestItem, index?: number, column?: IColumn) => {

      if (!column) {
         return null;
      }

      if (column.name === "Name") {
         //return <Stack horizontal>{<StepRefStatusIcon stepRef={ref} />}<Text>{jobDetails.nodeByStepId(stepId)?.name}</Text></Stack>;
         const status = handler.testStatus.get(item.testId);
         let color = dashboard.darktheme ? "#E0E0E0" : undefined;
         if (status?.success) {
            //color = scolors.get(StatusColor.Success)!;
         } else if (status?.error) {
            if (!dashboard.darktheme) {
               color = scolors.get(StatusColor.Failure)!;
            }
         }
         return <Stack style={{ height: 18 }} verticalFill={true} verticalAlign="center"><Text style={{ fontSize: 11, fontFamily: "Horde Open Sans Semibold", color: color }}>{item.name}</Text></Stack>;
      }

      if (column.name === "Graph") {

         return <Stack style={{ height: 18 }} verticalFill={true} verticalAlign="center"><SuiteGraph handler={handler} testId={item.testId} /></Stack>;
      }

      return null;

   };

   const testIds = new Set<TestId>();

   const testData = handler.getFilteredTestData();
   testData.forEach(d => testIds.add(d.testId));

   let tests: GetTestResponse[] = [];

   for (let testId of testIds) {
      const test = SuiteHandler.tests.get(testId);
      if (test) {
         tests.push(test);
      }
   }

   tests = tests.sort((a, b) => {
      const nameA = SuiteHandler.fixedTestNames.get(a.id)!;
      const nameB = SuiteHandler.fixedTestNames.get(b.id)!
      return nameA.localeCompare(nameB);
   });

   const items = tests.map(t => {
      return {
         key: `automation_suite_test_${t.displayName ?? t.name}_${id_counter++}`,
         name: SuiteHandler.fixedTestNames.get(t.id)!,
         testId: t.id
      }
   });

   const wheight = 1000;
   const flexBasic = windowSize.height < wheight ? "300px" : "400px";
   const maxHeight = windowSize.height < wheight ? 264 : 364;
   const metrics = handler.metrics;

   return <Stack key={`key_automation_suite_list_${id_counter++}`} className={hordeClasses.raised} style={{ flexBasis: flexBasic, flexShrink: 0 }}>
      <Stack >
         <Stack>
            {!!metrics && <Stack horizontal tokens={{childrenGap: 24}} style={{paddingBottom: 8}}>
               <Stack>
                  <Text>{`Tests: ${metrics.total}`}</Text>
               </Stack>
               {!!metrics.success && <Stack>
                  <Stack>
                     <Text>{`Success: ${metrics.success} / ${Math.ceil((metrics.success / metrics.total)* 100)}%`}</Text>
                  </Stack>
               </Stack>}
               {!!metrics.error && <Stack>
                  <Stack>
                     <Text>{`Error: ${metrics.error} / ${Math.ceil((metrics.error / metrics.total)* 100)}%`}</Text>
                  </Stack>
               </Stack>}
               {!!metrics.skipped && <Stack>
                  <Stack>
                     <Text>{`Skipped: ${metrics.skipped} / ${Math.ceil((metrics.skipped / metrics.total)* 100)}%`}</Text>
                  </Stack>
               </Stack>}
               {!!metrics.unspecified && <Stack>
                  <Stack>
                     <Text>{`Unspecified: ${metrics.unspecified} / ${Math.ceil((metrics.unspecified / metrics.total)* 100)}%`}</Text>
                  </Stack>
               </Stack>}
            </Stack>}
         </Stack>
         <Stack style={{ paddingLeft: 352 }}>
            <SuiteGraph handler={handler} />
         </Stack>
         <Stack>
            <div style={{ overflowY: 'auto', overflowX: 'hidden', maxHeight: maxHeight }} data-is-scrollable={true}>
               <DetailsList
                  styles={{ root: { overflowX: "hidden" } }}
                  key={`key_automation_suite_detaillist_${id_counter++}`}
                  className={styles.list}
                  compact={true}
                  isHeaderVisible={false}
                  items={items}
                  columns={columns}
                  layoutMode={DetailsListLayoutMode.justified}
                  onShouldVirtualize={() => true}
                  selectionMode={SelectionMode.none}
                  onRenderItemColumn={renderItem}
               />
            </div>
         </Stack>
      </Stack>
   </Stack>

});

const SuiteTestViewPanel: React.FC<{ handler: SuiteHandler }> = observer(({ handler }) => {

   return <Stack className={hordeClasses.raised} style={{ height: "100%", marginBottom: 8 }}>
      <Stack style={{ flexShrink: 0, height: "100%" }}>
         <SuiteTestView handler={handler} />
      </Stack>
   </Stack>
});



export const AutomationSuiteDetails: React.FC<{ suite: GetTestSuiteResponse, suiteRefs: GetTestDataRefResponse[], metaData: GetTestMetaResponse, onClose: () => void }> = observer(({ suite, suiteRefs, metaData, onClose }) => {

   const [state, setState] = useState<{ handler?: SuiteHandler }>({});

   let handler = state.handler;

   // subscribe
   if (!handler) {
      handler = new SuiteHandler(suite, [...suiteRefs], metaData);
      setState({ handler: handler });
      return null;
   }

   // subscribe
   if (handler.updated) { }

   const streamId = handler.suiteRefs.find(r => !!r.streamId)?.streamId;
   const streamName = projectStore.streamById(streamId)?.fullname ?? "Unknown Stream";

   return <Stack>
      <Modal isOpen={true} isBlocking={true} topOffsetFixed={true} styles={{ main: { padding: 8, width: 1700, backgroundColor: modeColors.background, hasBeenOpened: false, top: "24px", position: "absolute", height: "95vh" } }} className={hordeClasses.modal} onDismiss={() => { onClose() }}>
         <Stack className="horde-no-darktheme" style={{ height: "93vh" }}>
            <Stack style={{ height: "100%" }} >
               <Stack horizontal style={{ paddingLeft: 24, paddingTop: 12, paddingRight: 18 }}>
                  <Stack tokens={{ childrenGap: 4 }}>
                     <Text style={{ fontSize: 16, fontFamily: "Horde Open Sans Semibold" }}>{handler.metaData?.projectName} - {suite.name} - {handler.metaName}</Text>
                     <Text style={{ fontSize: 14, fontFamily: "Horde Open Sans Semibold" }}>{streamName}</Text>
                  </Stack>
                  <Stack grow />
                  <Stack>
                     <SuiteOperationsBar handler={handler} />
                  </Stack>
                  <Stack style={{ paddingLeft: 24, paddingTop: 0 }}>
                     <IconButton
                        style={{ height: "14px" }}
                        iconProps={{ iconName: 'Cancel' }}
                        onClick={() => onClose()} />
                  </Stack>
               </Stack>
               <Stack style={{ flexShrink: 0, flexGrow: 1, paddingLeft: 24, paddingRight: 24, paddingTop: 12 }}>
                  <div id="root_suite_test_list" style={{ position: "relative", flexShrink: 0, flexGrow: 1 }}>
                     {!handler.loaded && <Stack style={{ paddingTop: 16 }} horizontalAlign="center" tokens={{ childrenGap: 18 }}>
                        <Stack>
                           <Text variant="mediumPlus">Loading Test Suite</Text>
                        </Stack>
                        <Stack>
                           <Spinner size={SpinnerSize.large} />
                        </Stack>
                     </Stack>}
                     {!!handler.loaded && <Stack tokens={{ childrenGap: 18 }} style={{ height: "100%" }}>
                        <SuiteTestList handler={handler} />
                        <SuiteTestViewPanel handler={handler} />
                     </Stack>}
                  </div>
               </Stack>
            </Stack>
         </Stack>
      </Modal >
   </Stack>
});