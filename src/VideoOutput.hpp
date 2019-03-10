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

#ifndef __VBC_VIDEO_OUTPUT_HPP
#define __VBC_VIDEO_OUTPUT_HPP

#include <memory>
#include <string>

#include "Renderer.hpp"
#include "Tree.hpp"

class VideoOutput;
typedef std::shared_ptr<VideoOutput> VideoOutputPtr;

class VideoOutput {
public:
    struct Data;

private:
    std::shared_ptr<Data> d_;   ///< Internal data structures for rendering and encoding.

    size_t fps_n;               ///< Numerator of the frame rate.
    size_t fps_d;               ///< Denominator of the frame rate.
    size_t width;               ///< Width of output video in pixels.
    size_t height;              ///< Height of output video in pixels.
    std::string file;           ///< Path of output file.

    bool clock;                 ///< Render clock overlay.
    bool bounds;                ///< Render bounds overlay.
    double clock_adj;           ///< Additive adjustment for clock time.
    size_t cond_n;              ///< Numerator of time condensation factor.
    size_t cond_d;              ///< Denominator of time condensation factor.
    size_t text_halign;         ///< Horizontal alignment of text overlay.
    size_t text_valign;         ///< Vertical alignment of text overlay.

    bool push_video_data();

public:
    VideoOutput();
    VideoOutput(const VideoOutput&) = delete;
    VideoOutput(VideoOutput&& vidout) = delete;

    std::pair<size_t, size_t> get_dim() const { return std::make_pair(width, height); }                         ///< Returns dimensions of video.
    std::string get_file_path() const { return file; }                                                          ///< Returns output file name.

    bool get_clock() const;                                                                                     ///< Indicates whether a clock will be rendered.
    bool get_bounds() const;                                                                                    ///< Indicates whether bounds text will be rendered.
    std::pair<size_t, size_t> get_text_align() const { return std::make_pair(text_halign, text_valign); }       ///< Returns bounds overlay alignment flags

    size_t get_num_frames() const;      ///< Returns number of rendered frames.
    double get_frame_time() const;      ///< Returns duration of a single frame in seconds.
    double get_stream_time() const;     ///< Returns stream time at end of last rendered frame in seconds.
    double get_buffer_time() const;     ///< Returns stream time at end of last pushed frame in seconds
    double get_clock_time() const;      ///< Returns clock time at end of last rendered frame in seconds.

    std::pair<uint64_t, uint64_t> get_frame_rate() const;
    std::pair<uint64_t, uint64_t> get_time_condensation() const;
    double                        get_time_adjustment() const;

    void set_frame_rate(uint64_t num, uint64_t den);
    void set_time_condensation(uint64_t num, uint64_t den);
    void set_time_adjustment(double adj);
    void set_dim(size_t width, size_t height);
    void set_file_path(const std::string& file);
    void set_clock(bool on);
    void set_bounds(bool on);
    void set_text_align(size_t halign, size_t valign);

    void start();                               ///< Sets the renderer up, allocates resources, and starts rendering threads.
    bool push_frame(TreePtr tree);              ///< Renders a single frame of the tree and pushes it into the encoding pipeline.
    void stop(bool error = false);              ///< Shuts the renderer down and closes the output.
};

#endif /* end of include guard: __VBC_VIDEO_OUTPUT_HPP */
