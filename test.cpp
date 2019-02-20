#include <iostream>
#include <iomanip>

#include <gst/gst.h>

#include "src/Tree.hpp"
#include "src/VideoOutput.hpp"

int main(int argc, char** argv) {
    double last_time = -std::numeric_limits<double>::infinity();
    double current_time;

    gst_init(&argc, &argv);

    VideoOutputPtr vid_out = std::make_shared<VideoOutput>();
    TreePtr tree = std::make_shared<Tree>();

    vid_out->set_file_path("test.mp4");
    vid_out->set_dim(1024, 768);
    vid_out->set_frame_rate(30, 1);
    vid_out->start();

    tree->add_node(1, 0, 9);
    tree->add_node(2, 1, 9);
    tree->add_node(3, 1, 9);
    tree->add_node(4, 2, 4);
    tree->add_node(5, 2, 4);
    tree->add_node(6, 3, 4);

    for(size_t i = 0; i < 3000; ++i) {
        vid_out->push_frame(tree);

        current_time = vid_out->get_stream_time();
        if(current_time >= last_time + 5 || i == 2999) {
            char fill = std::cout.fill('0');
            std::cout << '\r' << std::setw(2) << int(current_time / 3600)
                      << ':' << std::setw(2) << int(std::fmod(current_time, 3600) / 60)
                      << ':' << std::setw(2) << int(std::fmod(current_time, 60))
                      << " - " << vid_out->get_num_frames() << " frames rendered" << std::flush;
            std::cout.fill(fill);
            last_time = current_time;
        }
    }
    std::cout << std::endl;

    vid_out->stop();

    gst_deinit();

    return 0;
}
