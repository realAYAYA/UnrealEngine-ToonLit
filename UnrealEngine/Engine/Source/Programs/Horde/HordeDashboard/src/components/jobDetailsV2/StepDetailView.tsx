// Copyright Epic Games, Inc. All Rights Reserved.

import { Stack } from "@fluentui/react";
import { observer } from "mobx-react-lite";
import { useState } from "react";
import { useHistory } from "react-router-dom";
import { hordeClasses } from "../../styles/Styles";
import { HistoryModal } from "../HistoryModal";
import { useQuery } from "../JobDetailCommon";
import { JobDetailArtifactsV2 } from "./JobDetailArtifactsV2";
import { HealthPanel } from "./JobDetailHealthV2";
import { StepHistoryPanel } from "./JobDetailStepHistory";
import { StepTrendsPanel } from "./JobDetailStepTrends";
import { JobDataView, JobDetailsV2 } from "./JobDetailsViewCommon";
import { StepsPanelV2 } from "./JobDetailViewSteps";
import { StepSummaryPanel } from "./StepDetailSummary";
import { StepTestReportPanel } from "./StepDetailTestPanel";
import { StepErrorPanel } from "./StepErrorPanel";


class StepDetailDataView extends JobDataView {

   filterUpdated() {
      this.updateReady();
   }

   detailsUpdated() {

      if (!this.details?.jobData) {
         return;
      }

      this.updateReady();
   }

}

JobDetailsV2.registerDataView("StepDetailDataView", (details: JobDetailsV2) => new StepDetailDataView(details));

const StepDetailViewInner: React.FC<{ jobDetails: JobDetailsV2, stepId: string }> = observer(({ jobDetails, stepId }) => {

   let [historyAgentId, setHistoryAgentId] = useState<string | undefined>(undefined);
   const history = useHistory();
   const query = useQuery();

   const dataView = jobDetails.getDataView<StepDetailDataView>("StepDetailDataView");

   dataView.initialize();

   dataView.subscribe();

   if (!historyAgentId && query.get("agentId")) {
      historyAgentId = query.get("agentId")!;
   }

   const jobData = jobDetails.jobData;

   if (!jobData) {
      return null;
   }

   const step = jobDetails.stepById(stepId);

   return <Stack>
      <HistoryModal agentId={historyAgentId} onDismiss={() => { history.replace(`/job/${jobData.id}?step=${stepId}`); setHistoryAgentId(undefined); }} />

      <Stack>
         <StepSummaryPanel jobDetails={jobDetails} stepId={stepId} />
      </Stack>
      <Stack>
         <HealthPanel jobDetails={jobDetails} />
      </Stack>
      <Stack>
         <StepErrorPanel jobDetails={jobDetails} stepId={stepId} showErrors={true} />
      </Stack>
      <Stack>
         <StepErrorPanel jobDetails={jobDetails} stepId={stepId} showErrors={false} />
      </Stack>
      <Stack>
         <StepsPanelV2 jobDetails={jobDetails} depStepId={stepId}/>
      </Stack>
      <Stack>
         <StepTestReportPanel jobDetails={jobDetails} stepId={stepId} />
      </Stack>
      {<Stack>
         <StepHistoryPanel jobDetails={jobDetails} stepId={stepId} />
      </Stack>}
      <Stack>
         <JobDetailArtifactsV2 jobDetails={jobDetails} stepId={stepId} />
      </Stack>
      {!!step && <Stack>
         <StepTrendsPanel jobDetails={jobDetails} />
      </Stack>}
   </Stack>
});


export const StepDetailView: React.FC<{ jobDetails: JobDetailsV2, stepId: string }> = ({ jobDetails, stepId }) => {
   return (
      <Stack className={hordeClasses.horde}>
         <StepDetailViewInner jobDetails={jobDetails} stepId={stepId} />
      </Stack>
   );
};

