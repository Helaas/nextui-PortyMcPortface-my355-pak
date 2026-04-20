#include <assert.h>

#include "../src/consent.h"
#include "../src/status.h"
#include "../src/ui.h"
#include "../src/updater_flow.h"

int main(void) {
    status_model model = {0};
    warning_consent consent = {0};

    model.action = ACTION_UPDATE;
    assert(decide_updater_flow(UI_UPDATER_CANCEL, &model, &consent) == FLOW_DECISION_EXIT);
    assert(decide_updater_flow(UI_UPDATER_PICK_VERSION, &model, &consent) == FLOW_DECISION_PICK_VERSION);

    model.action = ACTION_NONE;
    assert(decide_updater_flow(UI_UPDATER_RUN, &model, &consent) == FLOW_DECISION_EXIT);

    model.action = ACTION_INSTALL;
    assert(decide_updater_flow(UI_UPDATER_RUN, &model, &consent) == FLOW_DECISION_PROMPT_WARNING);
    assert(decide_updater_flow(UI_UPDATER_FORCE_REINSTALL, &model, &consent) == FLOW_DECISION_PROMPT_WARNING);

    consent.accepted = 1;
    assert(decide_updater_flow(UI_UPDATER_RUN, &model, &consent) == FLOW_DECISION_START_INSTALL);
    assert(decide_updater_flow(UI_UPDATER_FORCE_REINSTALL, &model, &consent) == FLOW_DECISION_START_INSTALL);

    return 0;
}
