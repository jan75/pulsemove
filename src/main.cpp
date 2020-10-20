#pragma clang diagnostic push
#pragma ide diagnostic ignored "hicpp-use-auto"
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <syslog.h>
#include "pulsemovecontext.h"

pa_threaded_mainloop *paThreadedMainloop = nullptr;
std::mutex mutex;
std::condition_variable event_cb_cv;
std::condition_variable server_info_cb_cv;
std::condition_variable sink_info_cb_cv;
std::condition_variable sink_input_info_cb_cv;
std::condition_variable sink_input_move_success_cb_cv;

static void subscribe_success_cb(pa_context *paContext, int success, void *userdata) {
    syslog(LOG_DEBUG, "subscribe_success_cb called");
}

static void subscribe_event_cb(pa_context *paContext, pa_subscription_event_type eventType, int idx, void *userdata) {
    syslog(LOG_DEBUG, "subscribe_event_cb called for index %d", idx);

    if((eventType & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) == PA_SUBSCRIPTION_EVENT_SINK_INPUT) {
        if((eventType & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_NEW) {
            PulseMoveContext *pulseMoveContext = static_cast<PulseMoveContext *>(userdata);
            pulseMoveContext->sink_input_idx = idx;

            event_cb_cv.notify_one();
        }
    }

}

static void server_info_cb(pa_context *paContext, const pa_server_info *serverInfo, void *userdata) {
    syslog(LOG_DEBUG, "server_info_cb called");

    PulseMoveContext *pulseMoveContext = static_cast<PulseMoveContext *>(userdata);
    std::string default_sink_name = serverInfo->default_sink_name;
    pulseMoveContext->default_sink_name = default_sink_name;

    server_info_cb_cv.notify_one();
}

static void sink_info_cb(pa_context *paContext, const pa_sink_info *paSinkInfo, int eol, void *userdata) {
    syslog(LOG_DEBUG, "sink_info_cb called. eol: %d", eol);

    if(eol > 0) {
        sink_info_cb_cv.notify_one();
        return;
    }

    if(paSinkInfo != nullptr) {
        PulseMoveContext *pulseMoveContext = static_cast<PulseMoveContext *>(userdata);
        int default_sink_idx = paSinkInfo->index;
        pulseMoveContext->default_sink_idx = default_sink_idx;
    }
}

static void sink_input_info_cb(pa_context *paContext, const pa_sink_input_info *paSinkInputInfo, int eol, void *userdata) {
    syslog(LOG_DEBUG, "sink_input_info_cb called. eol: %d", eol);

    if(eol > 0) {
        sink_input_info_cb_cv.notify_one();
        return;
    }

    if(paSinkInputInfo != nullptr) {
        PulseMoveContext *pulseMoveContext = static_cast<PulseMoveContext *>(userdata);
        int sink_idx = paSinkInputInfo->sink;
        std::string sink_input_name = paSinkInputInfo->name;

        pulseMoveContext->sink_idx = sink_idx;
        pulseMoveContext->sink_input_name.append(sink_input_name);
    }

}

static void sink_input_move_success_cb(pa_context *paContext, int success, void *userdata) {
    syslog(LOG_DEBUG, "sink_input_move_success_cb called. success: %d", success);

    sink_input_move_success_cb_cv.notify_one();
}

int main() {
    pa_proplist *paProplist = pa_proplist_new();
    pa_proplist_sets(paProplist, PA_PROP_APPLICATION_NAME, "PulseMove");

    paThreadedMainloop = pa_threaded_mainloop_new();
    pa_mainloop_api *paMainloopApi = pa_threaded_mainloop_get_api(paThreadedMainloop);
    pa_context *paContext = pa_context_new_with_proplist(paMainloopApi, nullptr, paProplist);

    int ret = pa_context_connect(paContext, nullptr, PA_CONTEXT_NOFAIL, nullptr);
    if(ret < 0) {
        syslog(LOG_ERR, "Could not connect to PulseAudio");
        return -1;
    }

    pa_context_state contextState = pa_context_get_state(paContext);
    pa_threaded_mainloop_start(paThreadedMainloop);

    while(contextState != PA_CONTEXT_READY) {

        pa_threaded_mainloop_lock(paThreadedMainloop);
        contextState = pa_context_get_state(paContext);
        pa_threaded_mainloop_unlock(paThreadedMainloop);

        if(contextState == PA_CONTEXT_FAILED || contextState == PA_CONTEXT_TERMINATED) {
            syslog(LOG_ERR, "Context failed or terminated");
            return -1;
        }
    }
    syslog(LOG_INFO, "Connected to PulseAudio server");


    PulseMoveContext *moveContext = new PulseMoveContext();

    pa_threaded_mainloop_lock(paThreadedMainloop);
    pa_context_set_subscribe_callback(
            paContext,
            reinterpret_cast<pa_context_subscribe_cb_t>(subscribe_event_cb),
            moveContext);

    pa_context_subscribe(
            paContext,
            PA_SUBSCRIPTION_MASK_SINK_INPUT,
            reinterpret_cast<pa_context_success_cb_t>(subscribe_success_cb),
            nullptr);
    pa_threaded_mainloop_unlock(paThreadedMainloop);


    std::unique_lock<std::mutex> lock(mutex);
    const std::chrono::seconds cb_timeout(5);

    while(true) {
        event_cb_cv.wait(lock);
        syslog(LOG_DEBUG, "New sink input added, checking whether it needs moving");

        pa_threaded_mainloop_lock(paThreadedMainloop);
        pa_context_get_server_info(
                paContext,
                server_info_cb,
                moveContext);
        pa_threaded_mainloop_unlock(paThreadedMainloop);


        std::cv_status server_info_cb_cv_res = server_info_cb_cv.wait_for(lock, cb_timeout);
        if(server_info_cb_cv_res == std::cv_status::timeout) {
            syslog(LOG_WARNING, "PulseAudio API request (callback) timed out after requesting server info. Quitting");
            break;
        }
        syslog(LOG_DEBUG, "Server info received");

        int sink_input_idx = moveContext->sink_input_idx;
        const char *default_sink_name = moveContext->default_sink_name.c_str();

        pa_threaded_mainloop_lock(paThreadedMainloop);
        pa_context_get_sink_info_by_name(
                paContext,
                default_sink_name,
                sink_info_cb,
                moveContext);
        pa_context_get_sink_input_info(
                paContext,
                sink_input_idx,
                sink_input_info_cb,
                moveContext);
        pa_threaded_mainloop_unlock(paThreadedMainloop);

        std::cv_status sink_info_cb_cv_res = sink_info_cb_cv.wait_for(lock, cb_timeout);
        std::cv_status sink_input_info_cb_cv_res = sink_input_info_cb_cv.wait_for(lock, cb_timeout);

        if(sink_info_cb_cv_res == std::cv_status::timeout || sink_input_info_cb_cv_res == std::cv_status::timeout) {
            syslog(LOG_WARNING, "PulseAudio API request (callback) timed out after requesting sink info or sink input info. Quitting");
            break;
        }
        int input_sink_idx = moveContext->sink_idx;
        int default_sink_idx = moveContext->default_sink_idx;
        syslog(LOG_DEBUG, "Info for default sink \"%s\" (%d) and info for sink input  \"%s\" (%d) received", moveContext->default_sink_name.c_str(), default_sink_idx, moveContext->sink_input_name.c_str(), input_sink_idx);

        if(input_sink_idx == default_sink_idx) {
            syslog(LOG_INFO, "Sink input \"%s\" (%d) already on default sink \"%s\" (%d)", moveContext->sink_input_name.c_str(), sink_input_idx, moveContext->default_sink_name.c_str(), default_sink_idx);
            moveContext->reset();
            continue;
        }

        pa_threaded_mainloop_lock(paThreadedMainloop);
        pa_context_move_sink_input_by_index(
                paContext,
                sink_input_idx,
                default_sink_idx,
                sink_input_move_success_cb,
                moveContext);
        pa_threaded_mainloop_unlock(paThreadedMainloop);

        std::cv_status sink_input_move_success_cb_cv_res = sink_input_move_success_cb_cv.wait_for(lock, cb_timeout);

        if(sink_input_move_success_cb_cv_res == std::cv_status::timeout) {
            syslog(LOG_WARNING, "PulseAudio API request (callback) timed out, is PulseAudio still running? Quitting");
            break;
        }

        syslog(LOG_INFO, "Sink input \"%s\" (%d) moved to default sink \"%s\" (%d)", moveContext->sink_input_name.c_str(), sink_input_idx, moveContext->default_sink_name.c_str(), default_sink_idx);
        moveContext->reset();
    }

    delete moveContext;
    pa_threaded_mainloop_stop(paThreadedMainloop);
    pa_threaded_mainloop_free(paThreadedMainloop);

    return 0;
}

#pragma clang diagnostic pop