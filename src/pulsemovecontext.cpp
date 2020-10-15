//
// Created by jan on 13.10.20.
//

#include "pulsemovecontext.h"

void PulseMoveContext::reset() {
    this->sink_input_idx = -1;
    this->default_sink_idx = -1;
    this->sink_idx = -1;
    this->default_sink_name.clear();
    this->sink_input_name.clear();
}
