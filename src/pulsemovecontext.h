//
// Created by jan on 13.10.20.
//

#ifndef PULSEMOVE_CPP_PULSEMOVECONTEXT_H
#define PULSEMOVE_CPP_PULSEMOVECONTEXT_H

#include <iostream>
#include <thread>
#include <pulse/mainloop.h>
#include <pulse/thread-mainloop.h>
#include <pulse/context.h>
#include <pulse/mainloop-api.h>
#include <pulse/subscribe.h>
#include <pulse/introspect.h>
#include <pulse/mainloop-signal.h>

class PulseMoveContext {
    private:


    public:
        int sink_input_idx;
        std::string sink_input_name;
        int default_sink_idx;
        std::string default_sink_name;
        int sink_idx;

        void reset();
};

#endif //PULSEMOVE_CPP_PULSEMOVECONTEXT_H
