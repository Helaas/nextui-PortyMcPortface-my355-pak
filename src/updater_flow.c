#include "updater_flow.h"

updater_flow_decision decide_updater_flow(
    ui_updater_choice choice,
    const status_model *model,
    const warning_consent *consent) {
    if (choice == UI_UPDATER_PICK_VERSION)
        return FLOW_DECISION_PICK_VERSION;

    if (choice == UI_UPDATER_CANCEL)
        return FLOW_DECISION_EXIT;

    if (choice == UI_UPDATER_RUN && model != NULL && model->action == ACTION_NONE)
        return FLOW_DECISION_EXIT;

    if ((choice == UI_UPDATER_RUN || choice == UI_UPDATER_FORCE_REINSTALL) &&
            (consent == NULL || !consent->accepted))
        return FLOW_DECISION_PROMPT_WARNING;

    if (choice == UI_UPDATER_RUN || choice == UI_UPDATER_FORCE_REINSTALL)
        return FLOW_DECISION_START_INSTALL;

    return FLOW_DECISION_EXIT;
}
