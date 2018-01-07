/* see bottom for original copyright */

// We use some GNU extensions (basename)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <sysexits.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define VERSION_STRING __DATE__

#if defined(__arm__)
extern "C" {

#include "bcm_host.h"
#include "interface/vcos/vcos.h"

#include "interface/mmal/mmal.h"
#include "interface/mmal/mmal_logging.h"
#include "interface/mmal/mmal_buffer.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_connection.h"
#include "interface/mmal/mmal_parameters_camera.h"

#include "RaspiCamControl.h"
#include "RaspiCLI.h"


}
#endif


#include "RaspiCapture.h"
#include "filethread.h"
#include <string>

#include "settings.h"
#include "plock.h"
#include "metrics.h"


#if defined(__arm__)

#define INLINE_HEADERS MMAL_TRUE
#define SEGMENT_SECONDS 20
//  10 == extreme quality, 40 == high compression
#define QUANT_PARAM 16


// Standard port setting for the camera component
#define MMAL_CAMERA_PREVIEW_PORT 0
#define MMAL_CAMERA_VIDEO_PORT 1
#define MMAL_CAMERA_CAPTURE_PORT 2

// Video format information
// 0 implies variable
#define VIDEO_FRAME_RATE_NUM 60
#define VIDEO_FRAME_RATE_DEN 1

/// Video render needs at least 2 buffers.
#define VIDEO_OUTPUT_BUFFERS_NUM 3

// Max bitrate we allow for recording
const int MAX_BITRATE_MJPEG = 25000000; // 25Mbits/s
const int MAX_BITRATE_LEVEL4 = 25000000; // 25Mbits/s
const int MAX_BITRATE_LEVEL42 = 62500000; // 62.5Mbits/s

/// Interval at which we check for an failure abort during capture
const int ABORT_INTERVAL = 100; // ms




extern "C" int mmal_status_to_int(MMAL_STATUS_T status);

// Forward
typedef struct RASPIVID_STATE_S RASPIVID_STATE;

#define MAX_INFOBUF_SIZE 1024

/** Struct used to pass information in encoder port userdata to callback
*/
typedef struct
{
    bool file_handle;                   /// File handle to write buffer data to.
    RASPIVID_STATE *pstate;              /// pointer to our state in case required in callback
    int abort;                           /// Set to 1 in callback if an error occurs to attempt to abort the capture
    int  flush_buffers;
    int64_t pts_base;
    int64_t dts_base;
    int64_t segment_start_time;
    char infobuf[MAX_INFOBUF_SIZE];
    int infobuf_size;
    CaptureParams *cparams;
} PORT_USERDATA;

/** Structure containing all state information for the current run
*/
struct RASPIVID_STATE_S
{
    int width;                          /// Requested width of image
    int height;                         /// requested height of image
    MMAL_FOURCC_T encoding;             /// Requested codec video encoding (MJPEG or H264)
    int bitrate;                        /// Requested bitrate
    int framerate;                      /// Requested frame rate (fps)
    int intraperiod;                    /// Intra-refresh period (key frame rate)
    int quantisationParameter;          /// Quantisation parameter - quality. Set bitrate 0 and set this for variable bitrate
    char *filename;                     /// filename of output file
    int immutableInput;                 /// Flag to specify whether encoder works in place or creates a new buffer.
                                        /// Result is preview can display either
                                        /// the camera output or the encoder output (with compression artifacts)
    int profile;                        /// H264 profile to use for encoding
    int level;                          /// H264 level to use for encoding

    int segmentSize;                    /// Segment mode In timed cycle mode, the amount of time the capture is off per cycle
    int segmentNumber;                  /// Current segment counter
    int shouldRecord;

    RASPICAM_CAMERA_PARAMETERS camera_parameters; /// Camera setup parameters

    MMAL_COMPONENT_T *camera_component;    /// Pointer to the camera component
    MMAL_COMPONENT_T *encoder_component;   /// Pointer to the encoder component
    MMAL_CONNECTION_T *preview_connection; /// Pointer to the connection from camera to raw buffer extract
    MMAL_CONNECTION_T *encoder_connection; /// Pointer to the connection from camera to encoder

    MMAL_POOL_T *encoder_pool; /// Pointer to the pool of buffers used by encoder output port
    MMAL_POOL_T *preview_pool; /// Pointer to the pool of buffers used by preview output port

    PORT_USERDATA callback_data;        /// Used to move data to the encoder callback

    int cameraNum;                       /// Camera number
    int settings;                        /// Request settings from the camera
    int sensor_mode;			            /// Sensor mode. 0=auto. Check docs/forum for modes selected by other values.
    int intra_refresh_type;              /// What intra refresh type to use. -1 to not set.
    int frame;
    int64_t starttime;
    int64_t lasttime;
};




/**
 * Assign a default set of parameters to the state passed in
 *
 * @param state Pointer to state structure to assign defaults to
 */
static void default_status(RASPIVID_STATE *state)
{
    if (!state)
    {
        vcos_assert(0);
        return;
    }

    // Default everything to zero
    memset(state, 0, sizeof(RASPIVID_STATE));

    // Now set anything non-zero
    state->width = 1920;       // Default to 1080p
    state->height = 1080;
    state->encoding = MMAL_ENCODING_H264;
    state->bitrate = 17000000; // This is a decent default bitrate for 1080p
    state->framerate = VIDEO_FRAME_RATE_NUM;
    state->intraperiod = -1;    // Not set
    state->quantisationParameter = 0;
    state->immutableInput = 1;
    state->profile = MMAL_VIDEO_PROFILE_H264_HIGH;
    state->level = MMAL_VIDEO_LEVEL_H264_4;

    state->segmentSize = 0; // don't segment
    state->segmentNumber = 0;
    state->shouldRecord = 0;

    state->cameraNum = 0;
    state->settings = 0;
    state->sensor_mode = 7;  //  640x480, full FOV, 4x4 binning, high FPS, on v1 camera

    state->intra_refresh_type = -1;

    state->frame = 0;

    // Set up the camera_parameters to default
    raspicamcontrol_set_defaults(&state->camera_parameters);
}




// Our main data storage vessel..
RASPIVID_STATE state;
static CaptureParams gparams;

/**
 *  buffer header callback function for camera control
 *
 *  Callback will dump buffer data to the specific file
 *
 * @param port Pointer to port from which callback originated
 * @param buffer mmal buffer header pointer
 */
static void camera_control_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    if (buffer->cmd == MMAL_EVENT_PARAMETER_CHANGED)
    {
        MMAL_EVENT_PARAMETER_CHANGED_T *param = (MMAL_EVENT_PARAMETER_CHANGED_T *)buffer->data;
        switch (param->hdr.id) {
            case MMAL_PARAMETER_CAMERA_SETTINGS:
                {
                    MMAL_PARAMETER_CAMERA_SETTINGS_T *settings = (MMAL_PARAMETER_CAMERA_SETTINGS_T*)param;
                    vcos_log_error("Exposure now %u, analog gain %u/%u, digital gain %u/%u",
                            settings->exposure,
                            settings->analog_gain.num, settings->analog_gain.den,
                            settings->digital_gain.num, settings->digital_gain.den);
                    vcos_log_error("AWB R=%u/%u, B=%u/%u",
                            settings->awb_red_gain.num, settings->awb_red_gain.den,
                            settings->awb_blue_gain.num, settings->awb_blue_gain.den
                            );
                }
                break;
        }
    }
    else if (buffer->cmd == MMAL_EVENT_ERROR)
    {
        vcos_log_error("No data received from sensor. Check all connections, including the Sunny one on the camera board");
    }
    else
    {
        vcos_log_error("Received unexpected camera control callback event, 0x%08x", buffer->cmd);
    }

    mmal_buffer_header_release(buffer);
}


static size_t bwrite(void const *d, size_t n, size_t m, std::vector<char> &f) {
    f.insert(f.end(), (char const *)d, (char const *)d + n * m);
    return m;
}

static bool write_riff_header(std::vector<char> &f) {
    unsigned char data[12] = {
        'R', 'I', 'F', 'F',
        0, 0, 0, 0,
        'h', '2', '6', '4',
    };
    return 12 == bwrite(data, 1, 12, f);
}

static bool write_chunk_header(std::vector<char> &f, char const *type, size_t size) {
    unsigned char data[8] = {
        (unsigned char)type[0], (unsigned char)type[1], (unsigned char)type[2], (unsigned char)type[3],
        (unsigned char)(size & 0xff), (unsigned char)((size >> 8) & 0xff),
        (unsigned char)((size >> 16) & 0xff), (unsigned char)((size >> 24) & 0xff)
    };
    return 8 == bwrite(data, 1, 8, f);
}

static bool write_rounded_chunk(std::vector<char> &f, char const *type, size_t size, void const *data) {
    if (!write_chunk_header(f, type, size)) {
        return false;
    }
    /* HACK ALERT! I know that raw buffer sizes are always aligned to 4 bytes, so I 
     * can round up and write past the end of requested data for purposes of alignment.
     */
    size_t ru = (size + 3) & ~3;
    if (ru != bwrite(data, 1, ru, f)) {
        return false;
    }
    return true;
}

static bool write_timeinfo_chunk(std::vector<char> &f, char const *type, void const *data, size_t dataSize) {
    uint64_t timestamp = metric::Collector::clock();
    if (!write_chunk_header(f, type, dataSize+8)) {
        return false;
    }
    if (8 != bwrite(&timestamp, 1, 8, f)) {
        return false;
    }
    if (dataSize > 0) {
        size_t ru = (dataSize + 3) & ~3;
        /* hack alert! round written data up to 4, so input buffer must always be 
         * padded to that size.
         */
        if (ru != bwrite(data, 1, ru, f)) {
            return false;
        }
    }
    return true;
}

static void write_info_header(std::vector<char> &f) {
    char info[512];
    sprintf(info, "date=%s time=%s", __DATE__, __TIME__);
    write_rounded_chunk(f, "info", strlen(info), info);
}


static void reap_results() {
    FileResult rslt[100];
    size_t n = get_results(rslt, 8);
    for (size_t i = 0; i != n; ++i) {
        if (rslt[i].result < 0) {
            fprintf(stderr, "filewrite error: file %d op %d error %d\n", rslt[i].name, rslt[i].type, rslt[i].result);
        }
    }
}

/**
 * Open a file based on the settings in state
 *
 * @param state Pointer to state
 */
static void open_filename(RASPIVID_STATE *pState, char *filename)
{
    char *tempname = NULL;

    char fnpat[1024];
    // Create a new filename string
    strncpy(fnpat, filename, sizeof(fnpat));
    fnpat[sizeof(fnpat)-20] = 0;
    char *dot = strrchr(fnpat, '.');
    if (dot) {
        *dot = 0;
        dot = strrchr(filename, '.');
    }
    if (!dot || strlen(dot) > 10) {
        dot = (char *)".riff";
    }
    char *p;
    while ((p = strchr(fnpat, '%')) != NULL) {
        *p = '_';
    }
    strcat(fnpat, "-%04d");
    strcat(fnpat, dot);
    pState->segmentNumber++;
    asprintf(&tempname, fnpat, pState->segmentNumber);

    new_file(pState->segmentNumber, tempname);
    std::vector<char> f;
    write_riff_header(f);
    write_info_header(f);
    write_file_vec(pState->segmentNumber, f);
    reap_results();

    free(tempname);
}


pthread_mutex_t encoder_mutex = PTHREAD_MUTEX_INITIALIZER;

bool inject_timeinfo_data(PORT_USERDATA *pData, uint16_t type, void const *data, size_t size) {
    PLock lock(encoder_mutex);
    if (sizeof(pData->infobuf)-pData->infobuf_size >= size+2) {
        memcpy(&pData->infobuf[pData->infobuf_size], &type, 2);
        memcpy(&pData->infobuf[pData->infobuf_size+2], data, size);
        pData->infobuf_size += size + 2;
        return true;
    }
    return false;
}

bool insert_metadata(uint16_t type, void const *data, uint16_t size) {
    if (!state.shouldRecord) {
        return false;
    }
    return inject_timeinfo_data(&state.callback_data, type, data, size);
}

/**
 *  buffer header callback function for encoder
 *
 *  Callback will dump buffer data to the specific file
 *
 * @param port Point
 * @param buffer mmal buffer header pointer
 */
static void encoder_buffer_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    PLock lock(encoder_mutex);
    MMAL_BUFFER_HEADER_T *new_buffer;

    // We pass our file handle and other stuff in via the userdata field.

    PORT_USERDATA *pData = (PORT_USERDATA *)port->userdata;
    if (pData != NULL) {
        RASPIVID_STATE *pstate = pData->pstate;
        int64_t current_time_ms = metric::Collector::clock()/1000;

        int bytes_written = buffer->length;
        Encoder_BufferCount.increment();
        Encoder_BufferSize.sample(bytes_written);

        if (pstate->shouldRecord) {

            //  keyframe?
            if (buffer->flags & MMAL_BUFFER_HEADER_FLAG_CONFIG) {
                //  start each new segment on a keyframe, so close if we've filled the time segment size
                if (pData->pstate->segmentSize && (current_time_ms > pData->segment_start_time + pData->pstate->segmentSize)) {
                    if (pData->file_handle) {
                        close_file(pstate->segmentNumber);
                        pData->file_handle = false;
                    }
                    pData->segment_start_time = current_time_ms;
                }
                if (!pData->file_handle) {
                    open_filename(pstate, pstate->filename);
                    pData->file_handle = true;
                    pData->pts_base = (buffer->pts == MMAL_TIME_UNKNOWN) ? 0 : buffer->pts;
                    pData->dts_base = (buffer->dts == MMAL_TIME_UNKNOWN) ? 0 : buffer->dts;
                    pData->segment_start_time = current_time_ms;
                    pData->infobuf_size = 0;
                } else {
                    flush_file(pstate->segmentNumber);
                }
            }

            //  If I have a file and have data, write data to the file!
            if (pData->file_handle && buffer->length) {

                mmal_buffer_header_mem_lock(buffer);
                if(buffer->flags & MMAL_BUFFER_HEADER_FLAG_CODECSIDEINFO) {
                    //We do not want to save inlineMotionVectors...
                    bytes_written = buffer->length;
                } else {
                    bytes_written = buffer->length;
                    std::vector<char> f;
                    if (!write_timeinfo_chunk(f, "time", pData->infobuf, pData->infobuf_size)) {
                        bytes_written = 0;
                    }
                    pData->infobuf_size = 0;
                    int64_t timestamps[2] = { buffer->pts - pData->pts_base, buffer->dts - pData->dts_base };
                    if (!write_rounded_chunk(f, "pdts", 16, timestamps)) {
                        bytes_written = 0;
                    }
                    if (!write_rounded_chunk(f, "h264", buffer->length, buffer->data)) {
                        bytes_written = 0;
                    }
                    write_file_vec(pstate->segmentNumber, f);
                    reap_results();
                }

                mmal_buffer_header_mem_unlock(buffer);

                if (bytes_written != (int)buffer->length) {
                    fprintf(stderr, "Failed to write buffer data (%d from %d)- aborting", bytes_written, buffer->length);
                    pstate->shouldRecord = false;
                    pData->file_handle = false;
                    close_file(pstate->segmentNumber);
                }
            }
        } else if (pData->file_handle) {
            close_file(pstate->segmentNumber);
            pData->file_handle = false;
        }
    }

    // release buffer back to the pool
    mmal_buffer_header_release(buffer);

    // and send one back to the port (if still open)
    if (port->is_enabled)
    {
        MMAL_STATUS_T status;

        new_buffer = mmal_queue_get(pData->pstate->encoder_pool->queue);

        if (new_buffer)
            status = mmal_port_send_buffer(port, new_buffer);

        if (!new_buffer || status != MMAL_SUCCESS)
            vcos_log_error("Unable to return a buffer to the encoder port");
    }
}


void recycle_buffer(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) {

    PORT_USERDATA *pData = (PORT_USERDATA *)port->userdata;

    // release buffer back to the pool
    mmal_buffer_header_release(buffer);

    // and send one back to the port (if still open)
    if (pData && port->is_enabled)
    {
        MMAL_STATUS_T status;
        MMAL_BUFFER_HEADER_T *new_buffer = NULL;

        new_buffer = mmal_queue_get(pData->pstate->preview_pool->queue);

        if (new_buffer)
            status = mmal_port_send_buffer(port, new_buffer);

        if (!new_buffer || status != MMAL_SUCCESS)
            vcos_log_error("Unable to return a buffer to the previewer port");
    }
}

/**
 *  buffer header callback function for preview
 *
 * @param port Pointer to port from which callback originated
 * @param buffer mmal buffer header pointer
 */
static void preview_buffer_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    PORT_USERDATA *pData = (PORT_USERDATA *)port->userdata;

    if (pData)
    {
        if (buffer->length)
        {
            Capture_BufferCount.increment();
            Capture_BufferSize.sample(buffer->length);
            mmal_buffer_header_mem_lock(buffer);
            pData->cparams->buffer_callback(buffer->data + buffer->offset, buffer->length, pData->cparams->buffer_cookie);
            mmal_buffer_header_mem_unlock(buffer);
        }
    }

    recycle_buffer(port, buffer);
}

/**
 * Create the camera component, set up its ports
 *
 * @param state Pointer to state control struct
 *
 * @return MMAL_SUCCESS if all OK, something else otherwise
 *
 */
static MMAL_STATUS_T create_camera_component(RASPIVID_STATE *state)
{
    MMAL_COMPONENT_T *camera = 0;
    MMAL_ES_FORMAT_T *format;
    MMAL_PORT_T *preview_port = NULL, *video_port = NULL, *still_port = NULL;
    int status;
    MMAL_PARAMETER_INT32_T camera_num =
    {{MMAL_PARAMETER_CAMERA_NUM, sizeof(camera_num)}, state->cameraNum};
    MMAL_POOL_T *pool = 0;

    /* Create the component */
    status = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA, &camera) ? MMAL_EINVAL : MMAL_SUCCESS;

    if (status != MMAL_SUCCESS)
    {
        vcos_log_error("Failed to create camera component");
        goto error;
    }

    //status = raspicamcontrol_set_all_parameters(camera, &state->camera_parameters);
    status = 0;

    status += raspicamcontrol_set_stereo_mode(camera->output[0], &state->camera_parameters.stereo_mode);
    status += raspicamcontrol_set_stereo_mode(camera->output[1], &state->camera_parameters.stereo_mode);
    status += raspicamcontrol_set_stereo_mode(camera->output[2], &state->camera_parameters.stereo_mode);

    if (status != MMAL_SUCCESS)
    {
        vcos_log_error("Could not set stereo mode : error %d", status);
        goto error;
    }

    status = mmal_port_parameter_set(camera->control, &camera_num.hdr);

    if (status != MMAL_SUCCESS)
    {
        vcos_log_error("Could not select camera : error %d", status);
        goto error;
    }

    if (!camera->output_num)
    {
        status = MMAL_ENOSYS;
        vcos_log_error("Camera doesn't have output ports");
        goto error;
    }

    status = mmal_port_parameter_set_uint32(camera->control, MMAL_PARAMETER_CAMERA_CUSTOM_SENSOR_CONFIG, state->sensor_mode);

    if (status != MMAL_SUCCESS)
    {
        vcos_log_error("Could not set sensor mode : error %d", status);
        goto error;
    }

    preview_port = camera->output[MMAL_CAMERA_PREVIEW_PORT];
    video_port = camera->output[MMAL_CAMERA_VIDEO_PORT];
    still_port = camera->output[MMAL_CAMERA_CAPTURE_PORT];

    if (state->settings)
    {
        MMAL_PARAMETER_CHANGE_EVENT_REQUEST_T change_event_request =
        {{MMAL_PARAMETER_CHANGE_EVENT_REQUEST, sizeof(MMAL_PARAMETER_CHANGE_EVENT_REQUEST_T)},
            MMAL_PARAMETER_CAMERA_SETTINGS, 1};

        status = mmal_port_parameter_set(camera->control, &change_event_request.hdr);
        if ( status != MMAL_SUCCESS )
        {
            vcos_log_error("No camera settings events");
        }
    }

    status = mmal_port_parameter_set_boolean(preview_port, MMAL_PARAMETER_ZERO_COPY, MMAL_TRUE);
    if (status != MMAL_SUCCESS) {
        vcos_log_error("Cannot turn on zero copy for preview");
        goto error;
    }

    status = mmal_port_parameter_set_boolean(video_port, MMAL_PARAMETER_ZERO_COPY, MMAL_TRUE);
    if (status != MMAL_SUCCESS) {
        vcos_log_error("Cannot turn on zero copy for video");
        goto error;
    }

    // Enable the camera, and tell it its control callback function
    status = mmal_port_enable(camera->control, camera_control_callback);

    if (status != MMAL_SUCCESS)
    {
        vcos_log_error("Unable to enable control port : error %d", status);
        goto error;
    }

    //  set up the camera configuration
    {
        MMAL_PARAMETER_CAMERA_CONFIG_T cam_config =
        {
            { MMAL_PARAMETER_CAMERA_CONFIG, sizeof(cam_config) },
            .max_stills_w = (uint32_t)state->width,
            .max_stills_h = (uint32_t)state->height,
            .stills_yuv422 = 0,
            .one_shot_stills = 0,
            .max_preview_video_w = (uint32_t)state->width,
            .max_preview_video_h = (uint32_t)state->height,
            .num_preview_video_frames = (uint32_t)(4 + vcos_max(0, (state->framerate-30)/10)),
            .stills_capture_circular_buffer_height = 0,
            .fast_preview_resume = 0,
            .use_stc_timestamp = MMAL_PARAM_TIMESTAMP_MODE_RAW_STC
        };
        mmal_port_parameter_set(camera->control, &cam_config.hdr);
    }

    // Now set up the port formats

    // Set the encode format on the Preview port
    // HW limitations mean we need the preview to be the same size as the required recorded output

    format = preview_port->format;

    if(state->camera_parameters.shutter_speed > 6000000)
    {
        MMAL_PARAMETER_FPS_RANGE_T fps_range = {{MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},
            { 50, 1000 }, {166, 1000}};
        mmal_port_parameter_set(preview_port, &fps_range.hdr);
    }
    else if(state->camera_parameters.shutter_speed > 1000000)
    {
        MMAL_PARAMETER_FPS_RANGE_T fps_range = {{MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},
            { 166, 1000 }, {999, 1000}};
        mmal_port_parameter_set(preview_port, &fps_range.hdr);
    }

    //enable dynamic framerate if necessary
    if (state->camera_parameters.shutter_speed)
    {
        if (state->framerate > 1000000./state->camera_parameters.shutter_speed)
        {
            state->framerate=0;
        }
    }

    format->encoding = MMAL_ENCODING_I420;
    format->encoding_variant = MMAL_ENCODING_I420;
    format->es->video.width = VCOS_ALIGN_UP(state->width, 32);
    format->es->video.height = VCOS_ALIGN_UP(state->height, 16);
    format->es->video.crop.x = (format->es->video.width - state->width)/2;
    format->es->video.crop.y = (format->es->video.height - state->height)/2;
    format->es->video.crop.width = state->width;
    format->es->video.crop.height = state->height;
    format->es->video.frame_rate.num = VIDEO_FRAME_RATE_NUM;
    format->es->video.frame_rate.den = VIDEO_FRAME_RATE_DEN;

    status = mmal_port_format_commit(preview_port);

    if (status != MMAL_SUCCESS)
    {
        vcos_log_error("camera viewfinder format couldn't be set");
        goto error;
    }

    preview_port->buffer_num = preview_port->buffer_num_recommended;
    preview_port->buffer_size = preview_port->buffer_size_recommended;
    if (preview_port->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM) {
        preview_port->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;
    }

    // Set the encode format on the video  port

    format = video_port->format;
    //format->encoding = MMAL_ENCODING_RGB24;
    //format->encoding_variant = 0;
    format->encoding = MMAL_ENCODING_OPAQUE;
    format->encoding_variant = MMAL_ENCODING_I420;

    if(state->camera_parameters.shutter_speed > 6000000)
    {
        MMAL_PARAMETER_FPS_RANGE_T fps_range = {{MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},
            { 50, 1000 }, {166, 1000}};
        mmal_port_parameter_set(video_port, &fps_range.hdr);
    }
    else if(state->camera_parameters.shutter_speed > 1000000)
    {
        MMAL_PARAMETER_FPS_RANGE_T fps_range = {{MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},
            { 167, 1000 }, {999, 1000}};
        mmal_port_parameter_set(video_port, &fps_range.hdr);
    }

    format->encoding = MMAL_ENCODING_OPAQUE;
    format->es->video.width = VCOS_ALIGN_UP(state->width, 32);
    format->es->video.height = VCOS_ALIGN_UP(state->height, 16);
    format->es->video.crop.x = (format->es->video.width - state->width)/2;
    format->es->video.crop.y = (format->es->video.height - state->height)/2;
    format->es->video.crop.width = state->width;
    format->es->video.crop.height = state->height;
    format->es->video.frame_rate.num = state->framerate;
    format->es->video.frame_rate.den = VIDEO_FRAME_RATE_DEN;

    status = mmal_port_format_commit(video_port);

    if (status != MMAL_SUCCESS)
    {
        vcos_log_error("camera video format couldn't be set");
        goto error;
    }

    // Ensure there are enough buffers to avoid dropping frames
    if (video_port->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM)
        video_port->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;


    // Set the encode format on the still  port

    format = still_port->format;

    format->encoding = MMAL_ENCODING_RGB24;
    format->encoding_variant = 0;
    // format->encoding = MMAL_ENCODING_OPAQUE;
    // format->encoding_variant = MMAL_ENCODING_I420;

    format->es->video.width = VCOS_ALIGN_UP(state->width, 32);
    format->es->video.height = VCOS_ALIGN_UP(state->height, 16);
    format->es->video.crop.x = (format->es->video.width - state->width)/2;
    format->es->video.crop.y = (format->es->video.height - state->height)/2;
    format->es->video.crop.width = state->width;
    format->es->video.crop.height = state->height;
    format->es->video.frame_rate.num = 0;
    format->es->video.frame_rate.den = 1;

    status = mmal_port_format_commit(still_port);

    if (status != MMAL_SUCCESS)
    {
        vcos_log_error("camera still format couldn't be set");
        goto error;
    }

    /* Ensure there are enough buffers to avoid dropping frames */
    if (still_port->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM)
        still_port->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;

    /* Enable component */
    status = mmal_component_enable(camera);

    if (status != MMAL_SUCCESS)
    {
        vcos_log_error("camera component couldn't be enabled");
        goto error;
    }

    // Note: this sets lots of parameters that were not individually addressed before.
    raspicamcontrol_set_all_parameters(camera, &state->camera_parameters);

    /* Create pool of buffer headers for the output port to consume */
    pool = mmal_port_pool_create(preview_port, preview_port->buffer_num, preview_port->buffer_size);

    if (!pool)
    {
        vcos_log_error("Failed to create buffer header pool for preview output port %s", preview_port->name);
    }

    state->preview_pool = pool;
    state->camera_component = camera;

    return (MMAL_STATUS_T)status;

error:

    if (camera)
        mmal_component_destroy(camera);

    return (MMAL_STATUS_T)status;
}

/**
 * Destroy the camera component
 *
 * @param state Pointer to state control struct
 *
 */
static void destroy_camera_component(RASPIVID_STATE *state)
{
    if (state->camera_component)
    {
        mmal_component_destroy(state->camera_component);
        state->camera_component = NULL;
    }
}

/**
 * Create the encoder component, set up its ports
 *
 * @param state Pointer to state control struct
 *
 * @return MMAL_SUCCESS if all OK, something else otherwise
 *
 */
static MMAL_STATUS_T create_encoder_component(RASPIVID_STATE *state)
{
    MMAL_COMPONENT_T *encoder = 0;
    MMAL_PORT_T *encoder_input = NULL, *encoder_output = NULL;
    MMAL_STATUS_T status;
    MMAL_POOL_T *pool;

    status = mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_ENCODER, &encoder);

    if (status != MMAL_SUCCESS)
    {
        vcos_log_error("Unable to create video encoder component");
        goto error;
    }

    if (!encoder->input_num || !encoder->output_num)
    {
        status = MMAL_ENOSYS;
        vcos_log_error("Video encoder doesn't have input/output ports");
        goto error;
    }

    encoder_input = encoder->input[0];
    encoder_output = encoder->output[0];

    status = mmal_port_parameter_set_boolean(encoder_output, MMAL_PARAMETER_ZERO_COPY, MMAL_TRUE);
    if (status != MMAL_SUCCESS) {
        vcos_log_error("Unable to use zero copy for encoder output");
    }

    // We want same format on input and output
    mmal_format_copy(encoder_output->format, encoder_input->format);

    // Only supporting H264 at the moment
    encoder_output->format->encoding = state->encoding;

    if(state->encoding == MMAL_ENCODING_H264)
    {
        if(state->level == MMAL_VIDEO_LEVEL_H264_4)
        {
            if(state->bitrate > MAX_BITRATE_LEVEL4)
            {
                fprintf(stderr, "Bitrate too high: Reducing to 25MBit/s\n");
                state->bitrate = MAX_BITRATE_LEVEL4;
            }
        }
        else
        {
            if(state->bitrate > MAX_BITRATE_LEVEL42)
            {
                fprintf(stderr, "Bitrate too high: Reducing to 62.5MBit/s\n");
                state->bitrate = MAX_BITRATE_LEVEL42;
            }
        }
    }
    else if(state->encoding == MMAL_ENCODING_MJPEG)
    {
        if(state->bitrate > MAX_BITRATE_MJPEG)
        {
            fprintf(stderr, "Bitrate too high: Reducing to 25MBit/s\n");
            state->bitrate = MAX_BITRATE_MJPEG;
        }
    }

    encoder_output->format->bitrate = state->bitrate;

    if (state->encoding == MMAL_ENCODING_H264)
        encoder_output->buffer_size = encoder_output->buffer_size_recommended;
    else
        encoder_output->buffer_size = 8<<10;    /* 8 MB seems sufficient -- was 256! */


    if (encoder_output->buffer_size < encoder_output->buffer_size_min)
        encoder_output->buffer_size = encoder_output->buffer_size_min;

    encoder_output->buffer_num = encoder_output->buffer_num_recommended;

    if (encoder_output->buffer_num < encoder_output->buffer_num_min)
        encoder_output->buffer_num = encoder_output->buffer_num_min;

    // We need to set the frame rate on output to 0, to ensure it gets
    // updated correctly from the input framerate when port connected
    encoder_output->format->es->video.frame_rate.num = 0;
    encoder_output->format->es->video.frame_rate.den = 1;

    // Commit the port changes to the output port
    status = mmal_port_format_commit(encoder_output);

    if (status != MMAL_SUCCESS)
    {
        vcos_log_error("Unable to set format on video encoder output port");
        goto error;
    }

    // Set the rate control parameter
    if (0)
    {
        MMAL_PARAMETER_VIDEO_RATECONTROL_T param = {{ MMAL_PARAMETER_RATECONTROL, sizeof(param)}, MMAL_VIDEO_RATECONTROL_DEFAULT};
        status = mmal_port_parameter_set(encoder_output, &param.hdr);
        if (status != MMAL_SUCCESS)
        {
            vcos_log_error("Unable to set ratecontrol");
            goto error;
        }

    }

    if (state->encoding == MMAL_ENCODING_H264 &&
            state->intraperiod != -1)
    {
        MMAL_PARAMETER_UINT32_T param = {{ MMAL_PARAMETER_INTRAPERIOD, sizeof(param)}, (uint32_t)state->intraperiod};
        status = mmal_port_parameter_set(encoder_output, &param.hdr);
        if (status != MMAL_SUCCESS)
        {
            vcos_log_error("Unable to set intraperiod");
            goto error;
        }
    }

    if (state->encoding == MMAL_ENCODING_H264 &&
            state->quantisationParameter)
    {
        MMAL_PARAMETER_UINT32_T param = {{ MMAL_PARAMETER_VIDEO_ENCODE_INITIAL_QUANT, sizeof(param)}, (uint32_t)state->quantisationParameter};
        status = mmal_port_parameter_set(encoder_output, &param.hdr);
        if (status != MMAL_SUCCESS)
        {
            vcos_log_error("Unable to set initial QP");
            goto error;
        }

        MMAL_PARAMETER_UINT32_T param2 = {{ MMAL_PARAMETER_VIDEO_ENCODE_MIN_QUANT, sizeof(param)}, (uint32_t)state->quantisationParameter};
        status = mmal_port_parameter_set(encoder_output, &param2.hdr);
        if (status != MMAL_SUCCESS)
        {
            vcos_log_error("Unable to set min QP");
            goto error;
        }

        MMAL_PARAMETER_UINT32_T param3 = {{ MMAL_PARAMETER_VIDEO_ENCODE_MAX_QUANT, sizeof(param)}, (uint32_t)state->quantisationParameter};
        status = mmal_port_parameter_set(encoder_output, &param3.hdr);
        if (status != MMAL_SUCCESS)
        {
            vcos_log_error("Unable to set max QP");
            goto error;
        }

    }

    if (state->encoding == MMAL_ENCODING_H264)
    {
        MMAL_PARAMETER_VIDEO_PROFILE_T  param;
        param.hdr.id = MMAL_PARAMETER_PROFILE;
        param.hdr.size = sizeof(param);

        param.profile[0].profile = (MMAL_VIDEO_PROFILE_T)state->profile;

        if((VCOS_ALIGN_UP(state->width,16) >> 4) * (VCOS_ALIGN_UP(state->height,16) >> 4) * state->framerate > 245760)
        {
            if((VCOS_ALIGN_UP(state->width,16) >> 4) * (VCOS_ALIGN_UP(state->height,16) >> 4) * state->framerate <= 522240)
            {
                fprintf(stderr, "Too many macroblocks/s: Increasing H264 Level to 4.2\n");
                state->level=MMAL_VIDEO_LEVEL_H264_42;
            }
            else
            {
                vcos_log_error("Too many macroblocks/s requested");
                goto error;
            }
        }

        param.profile[0].level = (MMAL_VIDEO_LEVEL_T)state->level;

        status = mmal_port_parameter_set(encoder_output, &param.hdr);
        if (status != MMAL_SUCCESS)
        {
            vcos_log_error("Unable to set H264 profile");
            goto error;
        }
    }

    if (mmal_port_parameter_set_boolean(encoder_input, MMAL_PARAMETER_VIDEO_IMMUTABLE_INPUT, state->immutableInput) != MMAL_SUCCESS)
    {
        vcos_log_error("Unable to set immutable input flag");
        // Continue rather than abort..
    }

    //set INLINE HEADER flag to generate SPS and PPS for every IDR if requested
    if (mmal_port_parameter_set_boolean(encoder_output, MMAL_PARAMETER_VIDEO_ENCODE_INLINE_HEADER, INLINE_HEADERS) != MMAL_SUCCESS)
    {
        vcos_log_error("failed to set INLINE HEADER FLAG parameters");
        // Continue rather than abort..
    }

    // Adaptive intra refresh settings
    if (state->encoding == MMAL_ENCODING_H264 &&
            state->intra_refresh_type != -1)
    {
        MMAL_PARAMETER_VIDEO_INTRA_REFRESH_T  param;
        param.hdr.id = MMAL_PARAMETER_VIDEO_INTRA_REFRESH;
        param.hdr.size = sizeof(param);

        // Get first so we don't overwrite anything unexpectedly
        status = mmal_port_parameter_get(encoder_output, &param.hdr);
        if (status != MMAL_SUCCESS)
        {
            vcos_log_warn("Unable to get existing H264 intra-refresh values. Please update your firmware");
            // Set some defaults, don't just pass random stack data
            param.air_mbs = param.air_ref = param.cir_mbs = param.pir_mbs = 0;
        }

        param.refresh_mode = (MMAL_VIDEO_INTRA_REFRESH_T)state->intra_refresh_type;

        //if (state->intra_refresh_type == MMAL_VIDEO_INTRA_REFRESH_CYCLIC_MROWS)
        //   param.cir_mbs = 10;

        status = mmal_port_parameter_set(encoder_output, &param.hdr);
        if (status != MMAL_SUCCESS)
        {
            vcos_log_error("Unable to set H264 intra-refresh values");
            goto error;
        }
    }

    //  Enable component
    status = mmal_component_enable(encoder);

    if (status != MMAL_SUCCESS)
    {
        vcos_log_error("Unable to enable video encoder component");
        goto error;
    }

    /* Create pool of buffer headers for the output port to consume */
    pool = mmal_port_pool_create(encoder_output, encoder_output->buffer_num, encoder_output->buffer_size);

    if (!pool)
    {
        vcos_log_error("Failed to create buffer header pool for encoder output port %s", encoder_output->name);
    }

    state->encoder_pool = pool;
    state->encoder_component = encoder;

    return status;

error:
    if (encoder)
        mmal_component_destroy(encoder);

    state->encoder_component = NULL;

    return status;
}

/**
 * Destroy the encoder component
 *
 * @param state Pointer to state control struct
 *
 */
static void destroy_encoder_component(RASPIVID_STATE *state)
{
    // Get rid of any port buffers first
    if (state->encoder_pool)
    {
        mmal_port_pool_destroy(state->encoder_component->output[0], state->encoder_pool);
    }

    if (state->encoder_component)
    {
        mmal_component_destroy(state->encoder_component);
        state->encoder_component = NULL;
    }
}

/**
 * Connect two specific ports together
 *
 * @param output_port Pointer the output port
 * @param input_port Pointer the input port
 * @param Pointer to a mmal connection pointer, reassigned if function successful
 * @return Returns a MMAL_STATUS_T giving result of operation
 *
 */
static MMAL_STATUS_T connect_ports(MMAL_PORT_T *output_port, MMAL_PORT_T *input_port, MMAL_CONNECTION_T **connection)
{
    MMAL_STATUS_T status;

    status =  mmal_connection_create(connection, output_port, input_port, MMAL_CONNECTION_FLAG_TUNNELLING | MMAL_CONNECTION_FLAG_ALLOCATION_ON_INPUT);

    if (status == MMAL_SUCCESS)
    {
        status =  mmal_connection_enable(*connection);
        if (status != MMAL_SUCCESS)
            mmal_connection_destroy(*connection);
    }

    return status;
}

/**
 * Checks if specified port is valid and enabled, then disables it
 *
 * @param port  Pointer the port
 *
 */
static void check_disable_port(MMAL_PORT_T *port)
{
    if (port && port->is_enabled)
        mmal_port_disable(port);
}

void set_recording(bool rec) {
    if (!state.shouldRecord == !rec) {
        return;
    }
    state.shouldRecord = rec ? 1 : 0;
    if (!rec) {
        state.callback_data.infobuf_size = 0;
    }
    fprintf(stderr, "recording is now %s\n", rec ? "on" : "off");
    Capture_Recording.set(rec);
}

volatile bool running = true;


/* reasonable for daylight */
float rgain = 1.7f;
float bgain = 1.5f;

MMAL_PORT_T *camera_preview_port = NULL;
MMAL_PORT_T *camera_video_port = NULL;
MMAL_PORT_T *camera_still_port = NULL;
MMAL_PORT_T *encoder_input_port = NULL;
MMAL_PORT_T *encoder_output_port = NULL;




int stop_capture()
{
    // Disable all our ports that are not handled by connections
    check_disable_port(camera_still_port);
    check_disable_port(camera_preview_port);
    check_disable_port(encoder_output_port);

    if (state.encoder_connection)
        mmal_connection_destroy(state.encoder_connection);

    // Can now close our file. Note disabling ports may flush buffers which causes
    // problems if we have already closed the file!
    if (state.callback_data.file_handle)
        close_file(state.segmentNumber);

    /* Disable components */
    if (state.encoder_component)
        mmal_component_disable(state.encoder_component);

    if (state.camera_component)
        mmal_component_disable(state.camera_component);

    fprintf(stderr, "destroying components\n");

    destroy_encoder_component(&state);
    destroy_camera_component(&state);

    return 0;
}

/**
 * main
 */
int setup_capture(CaptureParams const *cp)
{
    setenv("LOCALE", "C", 1);
    gparams = *cp;
    MMAL_STATUS_T status = MMAL_SUCCESS;

    // Register our application with the logging system
    vcos_log_register("RaspiVid", VCOS_LOG_CATEGORY);

    default_status(&state);
    state.camera_parameters.awbMode = MMAL_PARAM_AWBMODE_OFF;
    state.camera_parameters.awb_gains_r = rgain;
    state.camera_parameters.awb_gains_b = bgain;
    state.camera_parameters.flickerAvoidMode = MMAL_PARAM_FLICKERAVOID_60HZ;
    state.camera_parameters.exposureMode = MMAL_PARAM_EXPOSUREMODE_ANTISHAKE;
    //  TODO: REMOVEME
    state.camera_parameters.exposureMeterMode = MMAL_PARAM_EXPOSUREMETERINGMODE_AVERAGE;
    //state.camera_parameters.exposureMeterMode = MMAL_PARAM_EXPOSUREMETERINGMODE_SPOT;
    //state.camera_parameters.exposureMeterMode = MMAL_PARAM_EXPOSUREMETERINGMODE_MATRIX;
    //state.camera_parameters.exposureMeterMode = MMAL_PARAM_EXPOSUREMETERINGMODE_SPOT;
    state.camera_parameters.drc_level = MMAL_PARAMETER_DRC_STRENGTH_OFF;
    state.camera_parameters.videoStabilisation = 1;
    //  TODO: REMOVEME
    state.camera_parameters.shutter_speed = 12000;   //  microseconds
    //state.camera_parameters.shutter_speed = 8000;   //  microseconds
    //state.camera_parameters.shutter_speed = 4000;   //  microseconds
    state.width = gparams.width;
    state.height = gparams.height;
    state.bitrate = 0; //8000000;
    state.framerate = VIDEO_FRAME_RATE_NUM;
    state.intraperiod = 60;
    state.segmentSize = SEGMENT_SECONDS * 1000;
    state.quantisationParameter = get_setting_int("video.quantization", QUANT_PARAM); //0; // 10 == extreme quality; 40 == good compression
    state.profile = MMAL_VIDEO_PROFILE_H264_HIGH;
    state.level = MMAL_VIDEO_LEVEL_H264_4;
    state.filename = strdup(gparams.encodedFilename);

    if ((status = create_camera_component(&state)) != MMAL_SUCCESS)
    {
        vcos_log_error("%s: Failed to create camera component", __func__);
    }
    else if ((status = create_encoder_component(&state)) != MMAL_SUCCESS)
    {
        vcos_log_error("%s: Failed to create encode component", __func__);
        destroy_camera_component(&state);
    }
    else
    {
        camera_preview_port = state.camera_component->output[MMAL_CAMERA_PREVIEW_PORT];
        camera_video_port   = state.camera_component->output[MMAL_CAMERA_VIDEO_PORT];
        camera_still_port   = state.camera_component->output[MMAL_CAMERA_CAPTURE_PORT];
        encoder_input_port  = state.encoder_component->input[0];
        encoder_output_port = state.encoder_component->output[0];

        if (status != MMAL_SUCCESS)
            state.preview_connection = NULL;

        if (status == MMAL_SUCCESS)
        {
            // Now connect the camera to the encoder
            status = connect_ports(camera_video_port, encoder_input_port, &state.encoder_connection);

            if (status != MMAL_SUCCESS)
            {
                state.encoder_connection = NULL;
                vcos_log_error("%s: Failed to connect camera video port to encoder input", __func__);
                goto error;
            }
        }

        if (status == MMAL_SUCCESS)
        {
            // Set up our userdata - this is passed though to the callback where we need the information.
            state.callback_data.pstate = &state;
            state.callback_data.abort = 0;
            state.callback_data.pts_base = 0;
            state.callback_data.dts_base = 0;
            state.callback_data.segment_start_time = 0;
            state.callback_data.cparams = &gparams;

            camera_preview_port->userdata = (struct MMAL_PORT_USERDATA_T *)&state.callback_data;

            // Enable the splitter output port and tell it its callback function
            status = mmal_port_enable(camera_preview_port, preview_buffer_callback);

            if (status != MMAL_SUCCESS)
            {
                vcos_log_error("%s: Failed to setup splitter output port", __func__);
                goto error;
            }
        }

        if (status == MMAL_SUCCESS)
        {
            state.callback_data.file_handle = NULL;

            // Set up our userdata - this is passed though to the callback where we need the information.
            encoder_output_port->userdata = (struct MMAL_PORT_USERDATA_T *)&state.callback_data;

            // Enable the encoder output port and tell it its callback function
            status = mmal_port_enable(encoder_output_port, encoder_buffer_callback);

            if (status != MMAL_SUCCESS)
            {
                vcos_log_error("Failed to setup encoder output");
                goto error;
            }
        }

        if (status == MMAL_SUCCESS)
        {
            // Send all the buffers to the encoder output port
            {
                int num = mmal_queue_length(state.encoder_pool->queue);
                int q;
                for (q=0;q<num;q++)
                {
                    MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(state.encoder_pool->queue);

                    if (!buffer)
                        vcos_log_error("Unable to get a required buffer %d from pool queue", q);

                    if (mmal_port_send_buffer(encoder_output_port, buffer)!= MMAL_SUCCESS)
                        vcos_log_error("Unable to send a buffer to encoder output port (%d)", q);
                }
            }
        }

        if (status == MMAL_SUCCESS)
        {
            // Send all the buffers to the splitter output port
            int num = mmal_queue_length(state.preview_pool->queue);
            int q;
            for (q = 0; q < num; q++)
            {
                MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(state.preview_pool->queue);

                if (!buffer)
                    vcos_log_error("Unable to get a required buffer %d from pool queue", q);

                if (mmal_port_send_buffer(camera_preview_port, buffer)!= MMAL_SUCCESS)
                    vcos_log_error("Unable to send a buffer to splitter output port (%d)", q);
            }

            if (mmal_port_parameter_set_boolean(camera_video_port, MMAL_PARAMETER_CAPTURE, 1) != MMAL_SUCCESS)
            {
                // How to handle?
                vcos_log_error("Could not turn on capturing");
            }
            return 0;
        }
        else
        {
            vcos_log_error("%s: Failed to connect camera to preview", __func__);
        }

error:

        mmal_status_to_int(status);

        stop_capture();

    }
    return -1;
}


/*
   Copyright (c) 2013, Broadcom Europe Ltd
   Copyright (c) 2013, James Hughes
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:
 * Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.
 * Neither the name of the copyright holder nor the
 names of its contributors may be used to endorse or promote products
 derived from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * \file RaspiVid.c
 * Command line program to capture a camera video stream and encode it to file.
 * Also optionally display a preview/viewfinder of current camera input.
 *
 * \date 28th Feb 2013
 * \Author: James Hughes
 *
 * Description
 *
 * 3 components are created; camera, preview and video encoder.
 * Camera component has three ports, preview, video and stills.
 * This program connects preview and video to the preview and video
 * encoder. Using mmal we don't need to worry about buffers between these
 * components, but we do need to handle buffers from the encoder, which
 * are simply written straight to the file in the requisite buffer callback.
 *
 * We use the RaspiCamControl code to handle the specific camera settings.
 * We use the RaspiPreview code to get buffers of frame data.
 */

#endif  //  __arm__

