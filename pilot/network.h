#if !defined(network_h)
#define network_h

class FrameQueue;

bool load_network(char const *name);

FrameQueue *network_input_queue();

#endif  //  network_h

