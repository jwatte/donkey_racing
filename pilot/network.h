#if !defined(network_h)
#define network_h

class FrameQueue;

extern float gSteerAdjust;
extern float gThrottleBase;
extern float gThrottleAdjust;

bool load_network(char const *name, FrameQueue *output);
FrameQueue *network_input_queue();
void network_start();
void network_stop();

#endif  //  network_h

