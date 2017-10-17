#if !defined(metrics_h)
#define metrics_h

#include "metric.h"

extern metric::Counter Graphics_GlError;
extern metric::Flag Graphics_AlwaysMiplevel0;
extern metric::Flag Graphics_AlwaysTeximage;
extern metric::Flag Graphics_AlwaysRgba;
extern metric::Sampler Graphics_Fps;
extern metric::Counter Process_WarpBuffers;
extern metric::Counter Process_TotalBuffers;
extern metric::Sampler Process_WarpFps;
extern metric::Sampler Process_WarpPercent;
extern metric::Sampler Process_UnwarpTime;
extern metric::Sampler Process_NetFps;
extern metric::Counter Process_NetBuffers;
extern metric::Sampler Process_NetDuration;
extern metric::Counter Encoder_BufferCount;
extern metric::Sampler Encoder_BufferSize;
extern metric::Counter Capture_BufferCount;
extern metric::Sampler Capture_BufferSize;
extern metric::Flag Capture_Recording;
extern metric::Flag Serial_Error;
extern metric::Counter Serial_BytesReceived;
extern metric::Counter Serial_BytesSent;
extern metric::Counter Serial_CrcErrors;
extern metric::Flag Serial_UnknownMessage;
extern metric::Counter Serial_PacketsDecoded;
extern metric::Sampler Packet_SteerControlAge;
extern metric::Counter Packet_SteerControlCount;
extern metric::Sampler Packet_IBusPacketAge;
extern metric::Counter Packet_IBusPacketCount;
extern metric::Sampler Packet_TrimInfoAge;
extern metric::Counter Packet_TrimInfoCount;

bool start_metrics(char const *filename);
void stop_metrics();

#endif // metrics_h
