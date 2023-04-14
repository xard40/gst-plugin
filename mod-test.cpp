#include <iostream>
#include <vector>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <opencv2/opencv.hpp>

#define THERMAL_WIDTH 640
#define THERMAL_HEIGHT 480
#define THERMAL_FPS 30
#define RJPEG_QUALITY 80

// This function will be called by the appsink to get the encoded data
static GstFlowReturn on_new_sample_from_sink(GstElement* elt, gpointer data) {
    GstSample* sample = gst_app_sink_pull_sample(GST_APP_SINK(elt));
    GstBuffer* buffer = gst_sample_get_buffer(sample);

    if (buffer != NULL) {
        GstMapInfo map;
        if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
            std::vector<uchar> rjpeg_data(map.data, map.data + map.size);
            cv::Mat rjpeg_image(1, rjpeg_data.size(), CV_8UC1, rjpeg_data.data());

            // Do something with the encoded R-JPEG image, such as send it over the network

            gst_buffer_unmap(buffer, &map);
        }
    }

    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

int main() {
    // Initialize GStreamer
    gst_init(NULL, NULL);

    // Create a pipeline for thermal stream encoding using NVENC and R-JPEG
    GstElement* pipeline = gst_pipeline_new(NULL);

    GstElement* thermal_src = gst_element_factory_make("v4l2src", "thermal-src");
    GstElement* thermal_caps = gst_element_factory_make("capsfilter", "thermal-caps");
    GstElement* thermal_conv = gst_element_factory_make("videoconvert", "thermal-conv");
    GstElement* thermal_enc = gst_element_factory_make("nvv4l2h264enc", "thermal-enc");
    GstElement* thermal_mpegtsmux = gst_element_factory_make("mpegtsmux", "thermal-mpegtsmux");
    GstElement* thermal_rjpegenc = gst_element_factory_make("jpegenc", "thermal-rjpegenc");
    GstElement* thermal_sink = gst_element_factory_make("appsink", "thermal-sink");

    if (!pipeline || !thermal_src || !thermal_caps || !thermal_conv || !thermal_enc || !thermal_mpegtsmux || !thermal_rjpegenc || !thermal_sink) {
        std::cerr << "Failed to create GStreamer elements!" << std::endl;
        return 1;
    }

    // Set thermal source properties
    g_object_set(G_OBJECT(thermal_src), "device", "/dev/video0", NULL);

    // Set thermal capture properties
    GstCaps* thermal_caps_str = gst_caps_new_simple("video/x-raw",
        "format", G_TYPE_STRING, "GRAY8",
        "width", G_TYPE_INT, THERMAL_WIDTH,
        "height", G_TYPE_INT, THERMAL_HEIGHT,
        "framerate", GST_TYPE_FRACTION, THERMAL_FPS, 1, NULL);
    g_object_set(G_OBJECT(thermal_caps), "caps", thermal_caps_str, NULL);
    gst_caps_unref(thermal_caps_str);

    // Set thermal sink properties
    gst_app_sink_set_emit_signals(GST_APP_SINK(thermal_sink), true); 
    gst_app_sink_set_drop(GST_APP_SINK(thermal_sink), true); 
    gst_app_sink_set_max_buffers(GST_APP_SINK(thermal_sink), 1);
    //not sure if the minimum buffers are needed or not gst_app_sink_set_min_buffers(GST_APP_SINK(thermal_sink), 0.054); 
    GstCaps* thermal_sink_caps_str = gst_caps_new_simple("image/jpeg", "width", G_TYPE_INT, THERMAL_WIDTH, "height", G_TYPE_INT, THERMAL_HEIGHT, "framerate", GST_TYPE_FRACTION, THERMAL_FPS, 1, "quality", G_TYPE_INT, RJPEG_QUALITY, NULL); 
    g_object_set(G_OBJECT(thermal_sink), "caps", thermal_sink_caps_str, NULL); 
    gst_caps_unref(thermal_sink_caps_str); 
    g_signal_connect(thermal_sink, "new-sample", G_CALLBACK(on_new_sample_from_sink), NULL); 
    // Add elements to pipeline 
    gst_bin_add_many(GST_BIN(pipeline), thermal_src, thermal_caps, thermal_conv, thermal_enc, thermal_mpegtsmux, thermal_rjpegenc, thermal_sink, NULL); 
    // Link elements 
    if (!gst_element_link_many(thermal_src, thermal_caps, thermal_conv, thermal_enc, thermal_mpegtsmux, thermal_rjpegenc, thermal_sink, NULL)) { 
        std::cerr << "Failed to link GStreamer elements!" << std::endl; return 1; } 
        // Start pipeline 
        GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING); 
        if (ret == GST_STATE_CHANGE_FAILURE) { std::cerr << "Failed to start GStreamer pipeline!" << std::endl; return 1; } 
        // Wait for pipeline to finish 
        GstBus* bus = gst_element_get_bus(pipeline); 
        GstMessage* msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GstMessageType(GST_MESSAGE_ERROR | GST_MESSAGE_EOS)); 
        if (msg != NULL) { gst_message_unref(msg); } 
        // Stop pipeline 
        gst_element_set_state(pipeline, GST_STATE_NULL); 
        gst_object_unref(pipeline); 
        return 0; }