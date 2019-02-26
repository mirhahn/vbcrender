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

#include <cmath>
#include <chrono>
#include <csignal>
#include <iostream>
#include <iomanip>

#include <gst/gst.h>

#include "src/Tree.hpp"
#include "src/VbcReader.hpp"
#include "src/VideoOutput.hpp"

bool signal_terminate = false;
std::string signal_message;

void signal_handler(int sig) {
    if(sig == SIGTERM) {
        signal_message = "termination signal received";
        signal_terminate = true;
    }
    else if(sig == SIGINT) {
        signal_message = "user requested termination";
        signal_terminate = true;
    }
}

int main(int argc, char** argv) {
    typedef std::chrono::steady_clock Clock;
    typedef typename Clock::time_point TimePoint;
    typedef typename Clock::duration Duration;
    typedef std::chrono::duration<double> Seconds;

    Clock clock;
    TimePoint start_time;
    double current_runtime;
    double last_runtime = -std::numeric_limits<double>::infinity();
    double stream_time;

    gst_init(&argc, &argv);

    // Install signal handler
    std::signal(SIGTERM, signal_handler);
    std::signal(SIGINT, signal_handler);

    VideoOutputPtr vid_out = std::make_shared<VideoOutput>();
    VbcReaderPtr vbc_in = std::make_shared<VbcReader>(false, true);

    vbc_in->open("test.vbc");
    vbc_in->wait();
    if(vbc_in->get_state() == VbcReader::Error) {
        vbc_in->advance();
        gst_deinit();
        return 1;
    }
    TreePtr tree = vbc_in->get_tree();

    vid_out->set_file_path("test.mp4");
    vid_out->set_dim(1920, 1080);
    vid_out->set_frame_rate(30, 1);
    vid_out->start();

    start_time = clock.now();
    stream_time = vid_out->get_stream_time();
    while(vbc_in->get_state() == VbcReader::Processing) {
        // Check if termination has been requested
        if(signal_terminate) {
            std::cout << "SIGNAL: " << signal_message << std::endl;
            break;
        }

        if(!vbc_in->has_next()) {
            // Wait for the event queue to be populated
            vbc_in->wait();
        }
        else if(vbc_in->get_next_timestamp() > stream_time) {
            // Render a video frame
            vid_out->push_frame(tree);
            stream_time = vid_out->get_stream_time();

            // Find current runtime
            current_runtime = std::chrono::duration_cast<Seconds>(clock.now() - start_time).count();

            // Print current state
            if(current_runtime >= last_runtime + 5) {
                char fill = std::cout.fill('0');
                size_t prec = std::cout.precision(0);
                std::cout << std::fixed << current_runtime << " s - stream time "
                          << std::setw(2) << int(stream_time / 3600)
                          << ':' << std::setw(2) << int(std::fmod(stream_time, 3600) / 60)
                          << ':' << std::setw(2) << int(std::fmod(stream_time, 60))
                          << " - " << vid_out->get_num_frames() << " frames rendered" << std::endl;
                std::cout.precision(prec);
                std::cout.fill(fill);
                last_runtime = current_runtime;
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
