/*
 * vbcrender - Command line tool to render videos from VBC files.
 * Copyright (C) 2019 Mirko Hahn
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "Styles.hpp"
#include "VideoOutput.hpp"

#include <iomanip>
#include <iostream>
#include <thread>
#include <sstream>

#include <glib.h>
#include <gst/gst.h>
#include <GrContext.h>
#include <SkImageInfo.h>
#include <SkSurface.h>


struct VideoOutput::Data {
    Data()
        : img_info(),
          surface(nullptr),
          pool(NULL),
          pipeline(NULL),
          vidsrc(NULL),
          txtsrc(NULL),
          stream_time(0),
          num_frames(0),
          r_thread()
    {}
    Data(const Data&) = delete;
    Data(Data&&) = delete;
    ~Data() {
        if(r_thread.joinable()) {
            GstFlowReturn ret;
            g_signal_emit_by_name(vidsrc, "end-of-stream", &ret);
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
    }

    SkImageInfo         img_info;   ///< Image memory layout information.
    sk_sp<SkSurface>    surface;    ///< SKIA rendering surface.

    GstBufferPool*  pool;           ///< Buffer pool for video frames.
    GstElement*     pipeline;       ///< GStreamer encoding pipeline.
    GstElement*     vidsrc;         ///< Source element for rendered frames.
    GstElement*     txtsrc;         ///< Source element for overlay text.

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
        file_ext = g_utf8_casefold("avi", 3);
    }
    else {
        file_ext = g_utf8_casefold(&filename[last_period + 1], filename.size() - last_period - 1);
    }

    // Find typefinder of highest rank associated with the extension
    GstTypeFindFactory* best_type = NULL;
    GList* all_types = gst_type_find_factory_get_list();
    for(GList* l = all_types; l && !best_type; l = l->next) {
        GstTypeFindFactory *type = GST_TYPE_FIND_FACTORY(l->data);
        const gchar* const *exts = gst_type_find_factory_get_extensions(type);
        if(exts) {
            for(const gchar* const *e = exts; *e != NULL; ++e) {
                gchar* fold_ext = g_utf8_casefold(*e, -1);
                if(!strcmp(file_ext, fold_ext)) {
                    best_type = type;
                    break;
                }
                g_free(fold_ext);
            }
        }
    }

    // Obtain caps for this file type
    GstCaps* caps = best_type ? gst_caps_copy(gst_type_find_factory_get_caps(best_type)) : NULL;

#ifndef NDEBUG
    if(caps) {
        gchar* name = gst_caps_to_string(caps);
        std::cout << "AUTOPLUGGER: detected file caps as '" << name << '\'' << std::endl;
        g_free(name);
    }
#endif

    // Free allocated resources
    gst_plugin_feature_list_free(all_types);
    g_free(file_ext);

    return caps;
}


GstElement* create_bin_for_caps(const GstCaps* file_caps) {
    // Get a list of all muxer elements that can source the desired type
    GList* all_muxers = gst_element_factory_list_get_elements(GST_ELEMENT_FACTORY_TYPE_MUXER, GST_RANK_MARGINAL);
    GList* muxers = gst_element_factory_list_filter(all_muxers, file_caps, GST_PAD_SRC, FALSE);
    gst_plugin_feature_list_free(all_muxers);

    // Sort muxers by rank
    muxers = g_list_sort(muxers, gst_plugin_feature_rank_compare_func);

    // Obtain a list of all video encoders that can sink the desired type
    GList* encoders = gst_element_factory_list_get_elements(GST_ELEMENT_FACTORY_TYPE_ENCODER | GST_ELEMENT_FACTORY_TYPE_MEDIA_VIDEO, GST_RANK_MARGINAL);

    // Try all muxers in order of preference
    GstElementFactory* selected_muxer = NULL;
    GstElementFactory* selected_encoder = NULL;
    for(GList* it = muxers; it != NULL; it = it->next) {
        // Obtain the element factory of the current muxer
        GstElementFactory* muxer_factory = GST_ELEMENT_FACTORY(it->data);

        // Find static pad templates for the muxer
        const GList* pad_templates = gst_element_factory_get_static_pad_templates(muxer_factory);
        const GList* it_pad;
        for(it_pad = pad_templates; it_pad != NULL; it_pad = it_pad->next) {
            GstStaticPadTemplate* tmpl = (GstStaticPadTemplate*)it_pad->data;

            // Only consider sink pads
            if(tmpl->direction != GST_PAD_SINK) {
                continue;
            }

            // Filter all encoders that can attach to this pad
            GstCaps* pad_caps = gst_static_caps_get(&tmpl->static_caps);
            GList* pad_encoders = gst_element_factory_list_filter(encoders, pad_caps, GST_PAD_SRC, FALSE);
            gst_caps_unref(pad_caps);

            // Pick the highest ranked encoder
            guint highest_rank = GST_RANK_NONE;
            for(GList* it_enc = pad_encoders; it_enc != NULL; it_enc = it_enc->next) {
                GstElementFactory* enc = GST_ELEMENT_FACTORY(it_enc->data);
                guint rank = gst_plugin_feature_get_rank(GST_PLUGIN_FEATURE(enc));
                if(rank > highest_rank) {
                    selected_encoder = enc;
                    highest_rank = rank;
                }
            }

            // Free the pad-specific encoder list
            gst_plugin_feature_list_free(pad_encoders);

            // Terminate search if an encoder has been found
            if(selected_encoder) {
                break;
            }
        }

        // Terminate search if a suitable encoder is found
        if(selected_encoder) {
            selected_muxer = muxer_factory;
            break;
        }
    }

    // Free the remaining plugin feature lists
    gst_plugin_feature_list_free(encoders);
    gst_plugin_feature_list_free(muxers);

    // Return NULL if there is no suitable encoder
    if(!selected_encoder) {
        return NULL;
    }

    // Report selected encoder and muxer
#ifndef NDEBUG
    std::cout << "AUTOPLUGGER: selected encoder '" << gst_plugin_feature_get_name(GST_PLUGIN_FEATURE(selected_encoder)) << "'\n"
        << "AUTOPLUGGER: selected muxer '" << gst_plugin_feature_get_name(GST_PLUGIN_FEATURE(selected_muxer)) << '\'' << std::endl;
#endif

    // Create encoder and muxer and add them to a bin
    GstElement* bin = gst_bin_new("encodebin");
    GstElement* encoder = gst_element_factory_create(selected_encoder, "video-encoder");
    GstElement* muxer = gst_element_factory_create(selected_muxer, "format-muxer");

    // Fill the bin and link the elements
    gst_bin_add_many(GST_BIN(bin), encoder, muxer, NULL);
    gst_element_link(encoder, muxer);

    // Create ghost pads for all sources and sinks
    GstIteratorResult r;
    bool done = false;
    GValue item = G_VALUE_INIT;
    GstIterator* src_pads = gst_element_iterate_src_pads(muxer);
    while(!done) {
        switch(r = gst_iterator_next(src_pads, &item)) {
        case GST_ITERATOR_RESYNC:
            gst_iterator_resync(src_pads);
            break;
        case GST_ITERATOR_OK:
            {
                GstPad* target = GST_PAD(g_value_get_object(&item));
                GstPad* ghost = gst_ghost_pad_new(gst_pad_get_name(target), target);
                gst_element_add_pad(bin, ghost);
            }
            break;
        case GST_ITERATOR_DONE:
        case GST_ITERATOR_ERROR:
            done = true;
        }
    }
    g_value_unset(&item);
    gst_iterator_free(src_pads);

    done = false;
    item = G_VALUE_INIT;
    GstIterator* sink_pads = gst_element_iterate_sink_pads(encoder);
    while(!done) {
        switch(r = gst_iterator_next(sink_pads, &item)) {
        case GST_ITERATOR_RESYNC:
            gst_iterator_resync(sink_pads);
            break;
        case GST_ITERATOR_OK:
            {
                GstPad* target = GST_PAD(g_value_get_object(&item));
                GstPad* ghost = gst_ghost_pad_new(gst_pad_get_name(target), target);
                gst_element_add_pad(bin, ghost);
            }
            break;
        case GST_ITERATOR_DONE:
        case GST_ITERATOR_ERROR:
            done = true;
        }
    }
    g_value_unset(&item);
    gst_iterator_free(sink_pads);

    return bin;
}


VideoOutput::VideoOutput()
    : d_(),
      fps_n(30),
      fps_d(1),
      cond_n(1),
      cond_d(1),
      width(1920),
      height(1080),
      file("vbcrender.avi"),
      clock(false),
      bounds(false),
      text_halign(0),
      text_valign(2)
{}


double VideoOutput::get_frame_time() const {
    return double(fps_d * cond_d) / double(fps_n * cond_n);
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


void VideoOutput::set_time_condensation(size_t num, size_t den) {
    if(d_) {
        throw std::logic_error("attempt to set time condensation factor after rendering started");
    }
    
    cond_n = num;
    cond_d = den;
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


void VideoOutput::set_clock(bool on) {
    if(d_) {
        throw std::logic_error("attempt to switch clock state after rendering started");
    }

    clock = on;
}


void VideoOutput::set_bounds(bool on) {
    if(d_) {
        throw std::logic_error("attempt to switch bounds state after rendering started");
    }

    bounds = on;
}


void VideoOutput::set_text_align(size_t halign, size_t valign) {
    if(d_) {
        throw std::logic_error("attempt to switch text alignment after rendering started");
    }

    text_halign = halign;
    text_valign = valign;
}


void VideoOutput::start() {
    // Set up rendering pipeline if not yet created
    if(!d_) {
        d_ = std::make_unique<Data>();

        // Create rendering surface for SKIA
        d_->img_info = SkImageInfo::Make(width, height, kRGB_888x_SkColorType, kOpaque_SkAlphaType);
        d_->surface = SkSurface::MakeRaster(d_->img_info);

        // Try to deduce output caps based on file extension
        GstCaps* output_caps = get_caps_for_file(file);
        if(!output_caps) {
            throw std::runtime_error("failed to guess video file format");
        }

        // Define remaining caps
        GstCaps* input_video_caps = gst_caps_new_simple("video/x-raw",
            "format", G_TYPE_STRING, "RGBx",
            "width", G_TYPE_INT, (int)width,
            "height", G_TYPE_INT, (int)height,
            "framerate", GST_TYPE_FRACTION, (int)fps_n, (int)fps_d,
            NULL
        );
        GstCaps* input_text_caps = gst_caps_new_simple("text/x-raw",
            "format", G_TYPE_STRING, "utf8",
            NULL
        );

        // Dynamically generate an encoder bin
        GstElement* encodebin = create_bin_for_caps(output_caps);
        gst_caps_unref(output_caps);
        if(!encodebin) {
            throw std::runtime_error("failed to construct encoder for video file");
        }

        // Create remaining elements
        GstElement *filesink, *converter, *overlay;
        filesink = gst_element_factory_make("filesink", "file-output");
        converter = gst_element_factory_make("videoconvert", "video-convert");
        d_->vidsrc = gst_element_factory_make("appsrc", "video-source");
        d_->pipeline = gst_pipeline_new("render-pipeline");

        gst_bin_add_many(GST_BIN(d_->pipeline), d_->vidsrc, converter, encodebin, filesink, NULL);
        
        // Configure first elements and link where possible
        g_object_set(G_OBJECT(d_->vidsrc),
            "block" , TRUE              ,
            "caps"  , input_video_caps  ,
            "format", GST_FORMAT_TIME   ,
            NULL
        );
        g_object_set(G_OBJECT(filesink),
            "location", file.c_str(),
            NULL
        );
        gst_element_link_many(converter, encodebin, filesink, NULL);

        if(clock || bounds) {
            d_->txtsrc = gst_element_factory_make("appsrc", "overlay-text-source");
            overlay = gst_element_factory_make("textoverlay", "text-overlay");
            gst_bin_add_many(GST_BIN(d_->pipeline), d_->txtsrc, overlay, NULL);

            // Configure additional elements
            g_object_set(G_OBJECT(d_->txtsrc),
                "block" , TRUE              ,
                "caps"  , input_text_caps   ,
                "format", GST_FORMAT_TIME   ,
                NULL
            );
            g_object_set(G_OBJECT(overlay)  ,
                "halignment", (int)text_halign,
                "valignment", (int)text_valign,
                "line-alignment", (int)text_halign,
                NULL
            );
            gst_caps_unref(input_text_caps);
            gst_element_link_many(d_->vidsrc, overlay, converter, NULL);
            gst_element_link(d_->txtsrc, overlay);
        }
        else {
            gst_element_link(d_->vidsrc, converter);
        }

        // Create buffer pool
        d_->pool = gst_buffer_pool_new();
        GstStructure* pool_conf = gst_buffer_pool_get_config(d_->pool);
        gst_buffer_pool_config_set_params(pool_conf, input_video_caps, width * height * sizeof(uint32_t), 10, 100);
        gst_buffer_pool_set_config(d_->pool, pool_conf);
        gst_buffer_pool_set_active(d_->pool, true);
        gst_caps_unref(input_video_caps);

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
    SkRect tree_bb = tree->bounding_box();
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

    // Create a text buffer for the overlay
    if(d_->txtsrc) {
        std::ostringstream str;
        bool empty = true;

        if(clock) {
            guint64 timestamp;
            if(cond_n != cond_d) {
                timestamp = gst_util_uint64_scale(GST_BUFFER_PTS(buffer), cond_d, cond_n);
            }
            else {
                timestamp = GST_BUFFER_PTS(buffer);
            }

            const auto fill = str.fill('0');
            str << std::setw(2) << (timestamp / (3600 * GST_SECOND)) << ':'
                << std::setw(2) << ((timestamp % (3600 * GST_SECOND)) / (60 * GST_SECOND)) << ':'
                << std::setw(2) << ((timestamp % (60 * GST_SECOND)) / GST_SECOND) << '.'
                << std::setw(3) << ((timestamp % GST_SECOND) / (GST_SECOND / 1000));
            str.fill(fill);

            empty = false;
        }

        if(bounds) {
            double ub = tree->upper_bound();
            double lb = tree->lower_bound();

            if(std::isfinite(ub)) {
                if(!empty) {
                    str << '\n';
                }
                str << "UB = " << ub;
                empty = false;
            }

            if(std::isfinite(lb)) {
                if(!empty) {
                    str << '\n';
                }
                str << "LB = " << lb;
            }
        }

        // Create a buffer with the text data
        std::string msg = str.str();
        gchar* data = g_strndup(msg.c_str(), msg.size());
        GstBuffer* txtbuf = gst_buffer_new_wrapped(data, msg.size());

        // Set buffer metadata
        GST_BUFFER_DURATION(txtbuf) = d_->frame_duration;
        GST_BUFFER_PTS(txtbuf) = d_->stream_time;

        // Push buffer into the pipeline
        GstFlowReturn ret;
        g_signal_emit_by_name(d_->txtsrc, "push-buffer", txtbuf, &ret);
        gst_buffer_unref(txtbuf);
    }

    // Advance timestamps
    d_->stream_time += d_->frame_duration;
    ++d_->num_frames;

    // Push buffer into the pipeline
    GstFlowReturn ret;
    g_signal_emit_by_name(d_->vidsrc, "push-buffer", buffer, &ret);
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
    g_signal_emit_by_name(d_->vidsrc, "end-of-stream", &ret);
    if(ret != GST_FLOW_OK) {
        throw std::runtime_error("failed to send end-of-stream signal");
    }
    if(d_->r_thread.joinable()) {
        d_->r_thread.join();
    }
}
