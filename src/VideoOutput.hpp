#ifndef __VBC_VIDEO_OUTPUT_HPP
#define __VBC_VIDEO_OUTPUT_HPP

#include <memory>
#include <string>

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

public:
    VideoOutput();
    VideoOutput(const VideoOutput&) = delete;
    VideoOutput(VideoOutput&& vidout);

    std::pair<size_t, size_t> get_frame_rate() const { return std::make_pair(fps_n, fps_d); }   ///< Returns frame rate as fraction.
    std::pair<size_t, size_t> get_dim() const { return std::make_pair(width, height); }         ///< Returns dimensions of video.
    std::string get_file_path() const { return file; }                                          ///< Returns output file name.
    size_t get_num_frames() const;                                                              ///< Returns number of rendered frames.
    double get_frame_time() const;                                                              ///< Returns duration of a single frame in seconds.
    double get_stream_time() const;                                                             ///< Returns stream time at end of last rendered frame in seconds.

    void set_frame_rate(size_t num, size_t den);
    void set_dim(size_t width, size_t height);
    void set_file_path(const std::string& file);

    void start();                               ///< Sets the renderer up, allocates resources, and starts rendering threads.
    void push_frame(TreePtr tree);              ///< Renders a single frame of the tree and pushes it into the encoding pipeline.
    void stop(bool error = false);              ///< Shuts the renderer down and closes the output.
};

#endif /* end of include guard: __VBC_VIDEO_OUTPUT_HPP */
