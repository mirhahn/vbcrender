#include "Styles.hpp"
#include "VideoOutput.hpp"

#include <thread>

#include <glib.h>
#include <gst/gst.h>
#include <SkImageInfo.h>
#include <SkSurface.h>


struct VideoOutput::Data {
    Data()
        : img_info(),
          surface(nullptr),
          input_caps(NULL),
          pool(NULL),
          pipeline(NULL),
          appsrc(NULL),
          stream_time(0),
          num_frames(0),
          r_thread()
    {}
    Data(const Data&) = delete;
    Data(Data&&) = delete;
    ~Data() {
        if(r_thread.joinable()) {
            GstFlowReturn ret;
            g_signal_emit_by_name(appsrc, "end-of-stream", &ret);
            if(ret == GST_FLOW_OK) {
                r_thread.join();
            }
        }
        if(pipeline) {
            g_object_unref(G_OBJECT(pipeline));
        }
        if(pool) {
            g_object_unref(G_OBJECT(pool));
        }
        if(input_caps) {
            gst_caps_unref(input_caps);
        }
    }

    SkImageInfo         img_info;   ///< Image memory layout information.
    sk_sp<SkSurface>    surface;    ///< SKIA rendering surface.

    GstCaps*        input_caps;     ///< Input video data type.
    GstBufferPool*  pool;           ///< Buffer pool for video frames.
    GstElement*     pipeline;       ///< GStreamer encoding pipeline.
    GstElement*     appsrc;         ///< Source element for rendered frames.

    guint64         frame_duration; ///< Duration of single frame in nanoseconds.
    guint64         stream_time;    ///< Current stream timestamp.
    guint64         num_frames;     ///< Number of frames rendered so far.

    std::thread     r_thread;       ///< Separate render thread.
    GMainLoop*      loop;           ///< Main loop.
};


static void on_end_of_stream(GstBus* bus, GstMessage* message, VideoOutput::Data* user_data) {
    g_main_loop_quit(user_data->loop);
}


static void on_stream_error(GstBus* bus, GstMessage* message, VideoOutput::Data* user_data) {
    g_main_loop_quit(user_data->loop);
}


static GstCaps* get_caps_for_file(const std::string& filename) {
    // Extract file extension
    size_t last_period = filename.rfind('.');
    gchar* file_ext;
    if(last_period >= filename.size()) {
        file_ext = g_utf8_casefold("mp4", 3);
    }
    else {
        file_ext = g_utf8_casefold(&filename[last_period + 1], filename.size() - last_period - 1);
    }

    // Find typefinder of highest rank associated with the extension
    GstTypeFindFactory* best_type = NULL;
    guint best_rank = GST_RANK_NONE;

    GList* all_types = gst_type_find_factory_get_list();
    for(GList* l = all_types; l != NULL; l = l->next) {
        GstTypeFindFactory *type = GST_TYPE_FIND_FACTORY(l->data);
        guint rank = gst_plugin_feature_get_rank(GST_PLUGIN_FEATURE(type));

        if(!(rank > best_rank || !best_type)) {
            continue;
        }

        const gchar* const *exts = gst_type_find_factory_get_extensions(type);
        for(const gchar* const *e = exts; *e != NULL; ++e) {
            gchar* fold_ext = g_utf8_casefold(*e, -1);
            if(!strcmp(file_ext, fold_ext)) {
                best_type = type;
                best_rank = rank;
            }
            g_free(fold_ext);
        }
    }

    // Obtain caps for this file type
    GstCaps* caps = best_type ? gst_type_find_factory_get_caps(best_type) : NULL;

    // Free allocated resources
    gst_plugin_feature_list_free(all_types);
    g_free(file_ext);

    return caps;
}


VideoOutput::VideoOutput()
    : d_(),
      fps_n(30),
      fps_d(1),
      width(1920),
      height(1080),
      file("out.mp4")
{}


VideoOutput::VideoOutput(VideoOutput&& vidout)
    : d_(std::move(vidout.d_)),
      fps_n(vidout.fps_n),
      fps_d(vidout.fps_d),
      width(vidout.width),
      height(vidout.height),
      file(vidout.file)
{}


double VideoOutput::get_frame_time() const {
    return double(fps_d) / double(fps_n);
}


double VideoOutput::get_stream_time() const {
    return double(d_->stream_time) / double(GST_SECOND);
}


size_t VideoOutput::get_num_frames() const {
    return size_t(d_->num_frames);
}


void VideoOutput::set_frame_rate(size_t num, size_t den) {
    if(d_) {
        throw std::logic_error("attempt to set frame rate after rendering started");
    }
    
    fps_n = num;
    fps_d = den;
}


void VideoOutput::set_dim(size_t width, size_t height) {
    if(d_) {
        throw std::logic_error("attempt to set video dimension after rendering started");
    }

    this->width = width;
    this->height = height;
}


void VideoOutput::set_file_path(const std::string& file) {
    if(d_) {
        throw std::logic_error("attempt to set output path after rendering started");
    }

    this->file = file;
}


void VideoOutput::start() {
    // Set up rendering pipeline if not yet created
    if(!d_) {
        d_ = std::make_unique<Data>();

        // Create rendering surface for SKIA
        d_->img_info = SkImageInfo::Make(width, height, kRGB_888x_SkColorType, kOpaque_SkAlphaType);
        d_->surface = SkSurface::MakeRaster(d_->img_info);

        // Create caps object for generated frames
        d_->input_caps = gst_caps_new_simple("video/x-raw",
            "format", G_TYPE_STRING, "RGBx",
            "width", G_TYPE_INT, (int)width,
            "height", G_TYPE_INT, (int)height,
            "framerate", GST_TYPE_FRACTION, (int)fps_n, (int)fps_d,
            NULL
        );

        // Create buffer pool
        d_->pool = gst_buffer_pool_new();
        GstStructure* pool_conf = gst_buffer_pool_get_config(d_->pool);
        gst_buffer_pool_config_set_params(pool_conf, d_->input_caps, width * height * sizeof(uint32_t), 10, 100);
        gst_buffer_pool_set_config(d_->pool, pool_conf);
        gst_buffer_pool_set_active(d_->pool, true);

        // Create GStreamer pipeline
        d_->pipeline = gst_pipeline_new("encoding_pipeline");
        GstElement* filesink = gst_element_factory_make("filesink", "file_sink");
        GstElement* muxer = gst_element_factory_make("mp4mux", "stream_muxer");
        GstElement* encoder = gst_element_factory_make("x264enc", "video_encoder");
        GstElement* converter = gst_element_factory_make("videoconvert", "video_converter");
        GstElement* clock = gst_element_factory_make("timeoverlay", "time_overlay");
        d_->appsrc = gst_element_factory_make("appsrc", "video_source");

        g_object_set(d_->appsrc,
            "block", true,
            "caps", d_->input_caps,
            "format", GST_FORMAT_TIME,
            NULL
        );
        g_object_set(filesink,
            "location", file.c_str(),
            NULL
        );

        gst_bin_add_many(GST_BIN(d_->pipeline), d_->appsrc, clock, converter, encoder, muxer, filesink, NULL);
        gst_element_link_many(d_->appsrc, clock, converter, encoder, muxer, filesink, NULL);

        // Calculate frame duration
        d_->frame_duration = gst_util_uint64_scale(1, fps_d * GST_SECOND, fps_n);
    }

    // Wait for current render thread to die.
    if(d_->r_thread.joinable()) {
        d_->r_thread.join();
    }

    // Spin off new render thread.
    Data* data = d_.get();
    d_->r_thread = std::thread([data]() {
        // Create new main context and main loop.
        GMainContext* mainctx = g_main_context_new();
        g_main_context_push_thread_default(mainctx);
        data->loop = g_main_loop_new(mainctx, FALSE);

        // Install watch on GStreamer bus
        GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(data->pipeline));
        gst_bus_add_watch(bus, gst_bus_async_signal_func, NULL);

        // Install termination callbacks
        guint error_handler = g_signal_connect(bus, "message::error", (GCallback)on_stream_error, data);
        guint eos_handler = g_signal_connect(bus, "message::eos", (GCallback)on_end_of_stream, data);

        // Reset timestamps and frame counts
        data->stream_time = 0;
        data->num_frames = 0;

        // Transition the pipeline to playing state
        gst_element_set_state(data->pipeline, GST_STATE_READY);
        gst_element_set_state(data->pipeline, GST_STATE_PLAYING);

        // Run the main loop
        g_main_loop_run(data->loop);

        // Transition the pipeline to ready state
        gst_element_set_state(data->pipeline, GST_STATE_READY);

        // Remove termination callbacks
        g_signal_handler_disconnect(bus, error_handler);
        g_signal_handler_disconnect(bus, eos_handler);

        // Remove bus watch
        gst_bus_remove_watch(bus);

        // Destroy main context and main loop
        g_main_loop_unref(data->loop);
        g_main_context_pop_thread_default(mainctx);
        g_main_context_unref(mainctx);
        data->loop = NULL;
    });
}


void VideoOutput::push_frame(TreePtr tree) {
    // Retrieve canvas for rendering surface
    SkCanvas* canvas = d_->surface->getCanvas();

    // Update layout and get tree and canvas bounding boxes
    tree->update_layout();
    SkRect tree_bb = tree->get_bounding_box();
    SkRect window = SkRect::MakeIWH(d_->img_info.width(), d_->img_info.height()).makeInset(10, 10);

    // Adjust transformation to center tree
    canvas->setMatrix(SkMatrix::MakeRectToRect(tree_bb, window, SkMatrix::kCenter_ScaleToFit));

    // Draw the tree
    canvas->clear(background_color);
    tree->draw(canvas);

    // Acquire a buffer from GStreamer
    GstBuffer* buffer;
    if(gst_buffer_pool_acquire_buffer(d_->pool, &buffer, NULL) != GST_FLOW_OK) {
        throw std::runtime_error("failed to acquire buffer from pool");
    }

    // Copy pixels to the buffer
    GstMapInfo map_info;
    if(!gst_buffer_map(buffer, &map_info, GST_MAP_WRITE)) {
        gst_buffer_unref(buffer);
        throw std::runtime_error("failed to map buffer for writing");
    }
    SkPixmap pixmap(d_->img_info, map_info.data, d_->img_info.minRowBytes());
    d_->surface->readPixels(pixmap, 0, 0);
    gst_buffer_unmap(buffer, &map_info);

    // Attach timestamp information to the buffer
    GST_BUFFER_DURATION(buffer) = d_->frame_duration;
    GST_BUFFER_PTS(buffer) = d_->stream_time;
    d_->stream_time += d_->frame_duration;
    ++d_->num_frames;

    // Push buffer into the pipeline
    GstFlowReturn ret;
    g_signal_emit_by_name(d_->appsrc, "push-buffer", buffer, &ret);
    gst_buffer_unref(buffer);

    if(ret != GST_FLOW_OK) {
        throw std::runtime_error("could not push buffer to encoding pipeline");
    }
}


void VideoOutput::stop(bool error) {
    if(!d_) {
        return;
    }

    GstFlowReturn ret;
    g_signal_emit_by_name(d_->appsrc, "end-of-stream", &ret);
    if(ret != GST_FLOW_OK) {
        throw std::runtime_error("failed to send end-of-stream signal");
    }
    if(d_->r_thread.joinable()) {
        d_->r_thread.join();
    }
}
