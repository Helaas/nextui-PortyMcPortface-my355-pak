#ifndef UPDATER_FLOW_H
#define UPDATER_FLOW_H

#include "consent.h"
#include "status.h"
#include "ui.h"

typedef enum {
    FLOW_DECISION_EXIT = 0,
    FLOW_DECISION_PICK_VERSION,
    FLOW_DECISION_PROMPT_WARNING,
    FLOW_DECISION_START_INSTALL
} updater_flow_decision;

updater_flow_decision decide_updater_flow(
    ui_updater_choice choice,
    const status_model *model,
    const warning_consent *consent);

#endif
