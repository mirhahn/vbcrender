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
#include <sstream>
#include <unordered_map>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <gst/gst.h>

#include "Tree.hpp"
#include "VbcReader.hpp"
#include "VideoOutput.hpp"

namespace bfs = boost::filesystem;
namespace po = boost::program_options;


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

/// Program options
struct {
    bfs::path input_path;                       ///< Path of VBC input file.
    bfs::path output_path;                      ///< Path of video output file.

    size_t video_width;                         ///< Width of video output in pixels.
    size_t video_height;                        ///< Height of video output in pixels.
    size_t video_fps_n;                         ///< Numerator of frame rate.
    size_t video_fps_d;                         ///< Denominator of frame rate.
    size_t video_condense_n;                    ///< Numerator of time condensation factor.
    size_t video_condense_d;                    ///< Denominator of time condensation factor.

    double start_timestamp;                     ///< VBC timestamp to start rendering.
    double stop_timestamp;                      ///< VBC timestamp to stop rendering.

    bool                        clock;          ///< Render clock overlay.
    bool                        bounds;         ///< Render bound overlay.
    std::pair<size_t, size_t>   text_align;     ///< Alignment code for text overlay.

    double report_interval;                     ///< Interval for status updates in seconds.
    size_t header_repeat;                       ///< Number of status updates before header is repeated.
} program_options;


void print_usage_message(const char* executable, std::ostream& out) {
        // Find bare command
        std::string command = bfs::path(executable).filename().stem().c_str();

        // Print usage line
        out << "Usage: " << command << " [args...] [-o output-file] input-file\n";
}


std::string timestamp_string(double time) {
    std::ostringstream str;

    str.fill('0');
    str.precision(6);
    std::fixed(str);

    str << std::setw(2) << int(time / 3600) << ':'
        << std::setw(2) << std::abs(int(std::fmod(time, 3600) / 60)) << ':'
        << std::setw(9) << std::abs(std::fmod(time, 60));

    return str.str();
}


void parse_fraction(const std::string& str, size_t& num, size_t& den) {
    std::istringstream in(str);

    // Read numerator
    in >> std::ws >> num >> std::ws;
    if(in.bad() || in.fail()) {
        throw std::invalid_argument("not a valid fraction");
    }
    else if(in.eof()) {
        den = 1;
        return;
    }

    // Check if denominator exists
    auto ch = in.get();
    if(ch == '/') {
        in >> std::ws >> den >> std::ws;
    }
    else {
        throw std::invalid_argument("not a valid fraction");
    }

    if(in.bad() || in.fail()) {
        throw std::invalid_argument("not a valid fraction");
    }
}


void parse_pango_alignment(const std::string& str, std::pair<size_t, size_t>& align) {
    // Alignment words
    static std::unordered_map<std::string, size_t> halign_words {
        { "left",   0 },
        { "center", 1 },
        { "right",  2 },
    };
    static std::unordered_map<std::string, size_t> valign_words {
        { "baseline",   0 },
        { "bottom",     1 },
        { "top",        2 },
        { "middle",     4 },
    };

    std::string word;
    std::istringstream in(str);

    align.first = 0;
    align.second = 2;

    while(in >> std::ws >> word) {
        auto h_it = halign_words.find(word);
        if(h_it != halign_words.end()) {
            align.first = h_it->second;
        }
        else {
            auto v_it = valign_words.find(word);
            if(v_it != valign_words.end()) {
                align.second = v_it->second;
            }
            else {
                std::ostringstream out;
                out << "unknown word '" << word << '\'';
                throw std::invalid_argument(out.str());
            }
        }
    }
}


double parse_timestamp(const std::string& str) {
    double timestamp;
    double component;
    std::istringstream in(str);

    timestamp = 0.0;
    do {
        timestamp *= 60;
        if(!(in >> component)) {
            throw std::invalid_argument("not a valid timestamp");
        }
        timestamp += component;
    } while(in.get() == ':');
    return timestamp;
}


int parse_program_options(int argc, char** argv) {
    std::string text_align;
    std::string fps_frac;
    std::string condense_frac;
    std::string start_time;
    std::string end_time;

    // Describe program options
    po::options_description visible("Allowed options");
    visible.add_options()
        ("help,h", "produce help message")
        (
            "output,o",
            po::value<bfs::path>(&program_options.output_path)
                ->default_value(bfs::path("vbcrender.avi"), ""),
            "specify output file path"
        )(
            "width,w",
            po::value<size_t>(&program_options.video_width)
                ->default_value(1920, ""),
            "specify output video width"
        )(
            "height,h",
            po::value<size_t>(&program_options.video_height)
                ->default_value(1080, ""),
            "specify output video height"
        )(
            "fps",
            po::value<std::string>(&fps_frac),
            "specify output video frame rate (fraction)"
        )(
            "condense",
            po::value<std::string>(&condense_frac),
            "specify time condensation factor (fraction)"
        )(
            "start-time",
            po::value<std::string>(&start_time),
            "start video at given event time"
        )(
            "end-time",
            po::value<std::string>(&end_time),
            "end video at given event time"
        )(
            "clock",
            po::bool_switch(&program_options.clock),
            "render clock"
        )(
            "bounds",
            po::bool_switch(&program_options.bounds),
            "render current bounds"
        )(
            "overlay-pos",
            po::value<std::string>(&text_align),
            "specify position of text overlay"
        )
    ;
    po::options_description hidden("Hidden options");
    hidden.add_options()
        (
            "input-file",
            po::value<bfs::path>(&program_options.input_path)
        )
    ;
    po::options_description desc;
    desc.add(visible).add(hidden);

    // Add positional arguments
    po::positional_options_description p;
    p.add("input-file", 1);

    // Parse command line arguments
    po::variables_map vm;
    try{
        po::store(
            po::command_line_parser(argc, argv)
                .options(desc)
                .positional(p)
                .run(),
            vm
        );
        po::notify(vm);
    } catch(const std::exception& err) {
        std::cerr << "Error: " << err.what() << std::endl;
        return 1;
    }

    // Output help message if desired
    if(vm.count("help")) {
        print_usage_message(argv[0], std::cout);

        std::cout << "\nRenders a video from a VBC file.\n\n"
                  << visible << std::endl;
        return 1;
    }

    // Parse frame rate as fraction
    if(vm.count("fps")) {
        try {
            parse_fraction(fps_frac, program_options.video_fps_n, program_options.video_fps_d);
        } catch(const std::invalid_argument& err) {
            std::cerr << "Error parsing frame rate: " << err.what() << std::endl;
            return 1;
        }

        if(!program_options.video_fps_n || !program_options.video_fps_d) {
            std::cerr << "Error parsing frame rate: invalid numerator or denominator" << std::endl;
            return 1;
        }
    }
    else {
        program_options.video_fps_n = 30;
        program_options.video_fps_d = 1;
    }

    // Parse time condensation factor as fraction
    if(vm.count("condense")) {
        try {
            parse_fraction(condense_frac, program_options.video_condense_n, program_options.video_condense_d);
        } catch(const std::invalid_argument& err) {
            std::cerr << "Error parsing condensation factor: " << err.what() << std::endl;
            return 1;
        }

        if(!program_options.video_condense_n || !program_options.video_condense_d) {
            std::cerr << "Error parsing condensation factor: invalid numerator or denominator" << std::endl;
            return 1;
        }
    }
    else {
        program_options.video_condense_n = 1;
        program_options.video_condense_d = 1;
    }

    // Parse start timestamp
    if(vm.count("start-time")) {
        try {
            program_options.start_timestamp = parse_timestamp(start_time);
        }
        catch(const std::invalid_argument& err) {
            std::cerr << "Error parsing start time: " << err.what() << std::endl;
            return 1;
        }

        if(program_options.start_timestamp < 0) {
            std::cerr << "Warning: adjusting start time to 0.0" << std::endl;
            program_options.start_timestamp = 0;
        }
    }
    else {
        program_options.start_timestamp = 0;
    }

    // Parse end timestamp
    if(vm.count("end-time")) {
        try {
            program_options.stop_timestamp = parse_timestamp(end_time);
        }
        catch(const std::invalid_argument& err) {
            std::cerr << "Error parsing end time: " << err.what() << std::endl;
            return 1;
        }

        if(program_options.stop_timestamp <= program_options.start_timestamp) {
            std::cerr << "Warning: end time is before start time and will be ignored" << std::endl;
        }
    }
    else {
        program_options.stop_timestamp = 0;
    }

    // Parse overlay alignment
    if(vm.count("overlay-pos") > 0) {
        try {
            parse_pango_alignment(text_align, program_options.text_align);
        } catch(const std::invalid_argument& err) {
            std::cerr << "Error parsing overlay alignment: " << err.what() << std::endl;
            return 1;
        }
    }
    else {
        program_options.text_align = std::make_pair(0, 2);
    }

    // Throw an error if there is no input file
    if(!vm.count("input-file")) {
        print_usage_message(argv[0], std::cerr);
        std::cerr << "Error: expected input file" << std::endl;
        return 1;
    }

    // Set (currently) fixed default options
    program_options.report_interval = 5.0;
    program_options.header_repeat = 12;

    return 0;
}


void print_status_header(std::ostream& out) {
    out << "-----------+-----------------+-----------------+-----------------\n"
        << std::setw(10) << "runtime" << " | "
        << std::setw(15) << "clock_time" << " | "
        << std::setw(15) << "buffer_time" << " | "
        << std::setw(15) << "frames\n"
        << "-----------+-----------------+-----------------+-----------------" << std::endl;
}


void print_status_line(double runtime, double clock_time, double buffer_time, size_t frames, std::ostream& out) {
    const auto fill = out.fill();
    const auto prec = out.precision();
    out << std::fixed << std::setprecision(0) << std::setw(10) << runtime << " | "
        << std::setw(15) << timestamp_string(clock_time) << " | "
        << std::setw(15) << timestamp_string(buffer_time) << " | "
        << std::setfill(fill) << std::setw(15) << frames << std::endl;
    out.precision(prec);
}


int main(int argc, char** argv) {
    typedef std::chrono::steady_clock Clock;
    typedef typename Clock::time_point TimePoint;
    typedef typename Clock::duration Duration;
    typedef std::chrono::duration<double> Seconds;

    Clock clock;
    TimePoint start_time;
    size_t last_report_cycle = 0;
    size_t current_report_cycle = 0;
    size_t reports_given = 0;
    double current_runtime;
    double buffer_time;

    // Initialize GStreamer
    gst_init(&argc, &argv);

    // Parse remaining command line arguments
    if(parse_program_options(argc, argv)) {
        gst_deinit();
        return 1;
    }

    // Install signal handler
    std::signal(SIGTERM, signal_handler);
    std::signal(SIGINT, signal_handler);

    VideoOutputPtr vid_out = std::make_shared<VideoOutput>();
    VbcReaderPtr vbc_in = std::make_shared<VbcReader>(false, true);

    // Configure VBC input
    vbc_in->open(program_options.input_path.c_str());
    vbc_in->wait();
    if(vbc_in->get_state() == VbcReader::Error) {
        vbc_in->advance();
        gst_deinit();
        return 1;
    }
    TreePtr tree = vbc_in->get_tree();

    // Configure video output
    vid_out->set_file_path(program_options.output_path.c_str());
    vid_out->set_dim(program_options.video_width, program_options.video_height);
    vid_out->set_frame_rate(
        program_options.video_fps_n,
        program_options.video_fps_d
    );
    vid_out->set_time_condensation(
        program_options.video_condense_n,
        program_options.video_condense_d
    );
    vid_out->set_time_adjustment(program_options.start_timestamp);
    vid_out->set_clock(program_options.clock);
    vid_out->set_bounds(program_options.bounds);
    vid_out->set_text_align(program_options.text_align.first, program_options.text_align.second);
    vid_out->start();

    start_time = clock.now();
    buffer_time = vid_out->get_buffer_time();
    while(vbc_in->get_state() == VbcReader::Processing) {
        // Check if termination has been requested
        if(signal_terminate) {
            std::cout << "SIGNAL: " << signal_message << std::endl;
            break;
        }

        // Determine whether new frame should be pushed
        if(vbc_in->has_next()) {
            bool should_show = vbc_in->get_next_timestamp() > buffer_time + program_options.start_timestamp;

            // Push frame if necessary
            if(should_show) {
                vid_out->push_frame(tree);
                buffer_time = vid_out->get_buffer_time();
            }

            // Determine whether state should be advanced
            bool should_advance = vbc_in->get_next_timestamp() <= buffer_time + program_options.start_timestamp;

            // Advance tree state if necessary
            if(should_advance && !vbc_in->advance()) {
                std::cerr << "ERROR: Could not advance VBC state." << std::endl;
                break;
            }
        }
        else {
            vbc_in->wait();
        }

        // Find current runtime and calculate report cycles
        current_runtime = std::chrono::duration_cast<Seconds>(clock.now() - start_time).count();
        current_report_cycle = size_t(current_runtime / program_options.report_interval);

        // Print current state
        if(current_report_cycle != last_report_cycle) {
            if(reports_given == 0 || (program_options.header_repeat && reports_given % program_options.header_repeat == 0)) {
                print_status_header(std::cout);
            }
            print_status_line(
                current_runtime,
                vid_out->get_clock_time(),
                buffer_time,
                vid_out->get_num_frames(),
                std::cout
            );
            ++reports_given;
            last_report_cycle = current_report_cycle;
        }

        // Check for early termination
        if(program_options.stop_timestamp > program_options.start_timestamp && buffer_time >= program_options.stop_timestamp - program_options.start_timestamp) {
            break;
        }
    }

    vbc_in->close(); 
    vid_out->stop();

    gst_deinit();

    return 0;
}
