# PulseMove 
A small program running in a loop and checking for PulseAudio events where a new sink input (~ audio stream) is 
added. For each stream checks whether the stream is currently on the default sink. If not the sink input is moved to 
the default sink.

I'm not an experienced C / C++ programmer, especially when it comes to working with system libraries. PulseMove is a 
bit of a mix between C and C++ since the PulseAudio API is C, as well as possibly other glaring issues I don't 
recognize. It's enough for now as it is and it seems to work fine so far. 

_Background: I often switch PulseAudio output between my sound card and my GPU (which is connected via 
HDMI to my TV). I use a script to change the default sink and move currently registered sink inputs to the new default 
sink. Other, currently not active sink inputs with their last sink saved aren't moved though, and that's where this 
little tool comes in._

# Usage 
Run the binary directly or use the included `pulsemove.service` file to create a (user) systemd service.

# Build 
Build using `cmake`. Dependencies are:
- pulseaudio (pulse) 
- pthreads 
