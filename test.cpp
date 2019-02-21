#include <iostream>
#include <iomanip>

#include <gst/gst.h>

#include "src/Tree.hpp"
#include "src/VbcReader.hpp"
#include "src/VideoOutput.hpp"

int main(int argc, char** argv) {
    double last_time = -std::numeric_limits<double>::infinity();
    double current_time;

    gst_init(&argc, &argv);

    VideoOutputPtr vid_out = std::make_shared<VideoOutput>();
    VbcReaderPtr vbc_in = std::make_shared<VbcReader>(true);

    vid_out->set_file_path("test.mp4");
    vid_out->set_dim(1024, 768);
    vid_out->set_frame_rate(30, 1);
    vid_out->start();

    vbc_in->open("test.vbc");
    TreePtr tree = vbc_in->get_tree();

    current_time = vid_out->get_stream_time();
    while(vbc_in->get_state() == VbcReader::Processing) {
        if(!vbc_in->has_next()) {
            // Wait for the event queue to be populated
            vbc_in->wait();
        }
        else if(vbc_in->get_next_timestamp() > current_time) {
            // Render a video frame
            vid_out->push_frame(tree);
            current_time = vid_out->get_stream_time();

            // Print current state
            if(current_time >= last_time + 5) {
                char fill = std::cout.fill('0');
                std::cout << std::setw(2) << int(current_time / 3600)
                          << ':' << std::setw(2) << int(std::fmod(current_time, 3600) / 60)
                          << ':' << std::setw(2) << int(std::fmod(current_time, 60))
                          << " - " << vid_out->get_num_frames() << " frames rendered" << std::endl;
                std::cout.fill(fill);
                last_time = current_time;
            }
        }
        else if(!vbc_in->advance()) {
            std::cerr << "ERROR: Could not advance VBC state." << std::endl;
            break;
        }
    }

    vbc_in->close();
    vid_out->stop();

    gst_deinit();

    return 0;
}
