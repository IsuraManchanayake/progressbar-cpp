#include <thread>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <utility>
#include <sys/ioctl.h>
#include <unistd.h>

#include "windowhistory.hpp"

template<typename T>
class ProgressBar {
public:
    ProgressBar(const T& progress, const T& max_progress) 
        : progress(progress), max_progress(max_progress) {}

    void init() {
        start = std::chrono::steady_clock::now();
        history.emplace(start, progress);
        do {
            print_progressbar(current_progress());
            std::this_thread::sleep_for(std::chrono::milliseconds(sampling_freq_millis));
        } while(progress != max_progress);
        print_progressbar(1.0, true);
        time_point end = std::chrono::steady_clock::now();
        std::cout << "\nJob completed in " << duration_sec(start, end) << " s" << std::endl;
    }

private:
    typedef std::chrono::time_point<std::chrono::steady_clock> time_point;
    const T& progress;
    const T& max_progress;
    const size_t bar_width = 50;
    const size_t sampling_freq_millis = 100;
    WindowHistory<std::pair<time_point, T>, 20> history;
    double current_speed;
    time_point start;
    
    double current_progress() const {
        return static_cast<double>(progress) / max_progress;
    }

    size_t map_progress(size_t upper) const {
        return current_progress() * upper;
    }

    double calculate_speed() const {
        T window_progress = history.end().second - history.beg().second;
        double time_taken = duration_sec(history.beg().first, history.end().first);
        return window_progress / time_taken;
    }

    static double duration_sec(const time_point& t1, const time_point& t2) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() / 1000.0;
    }

    void print_progressbar(double fraction, bool terminating = false) {
        std::ostringstream ss;
        std::ios_base::fmtflags f(ss.flags());
        ss << std::fixed << std::setprecision(2);
        time_point current = std::chrono::steady_clock::now();
        history.emplace(current, progress);
        if(!terminating) { current_speed = calculate_speed(); }
        double current_time = duration_sec(start, current);
        double estimated_remaining_time = (max_progress - progress) / current_speed;
        double current_progress_percentage = (progress * 100) / max_progress;
        ss << "[Elapsed: " << current_time << " s] ";
        ss << "|";
        for(size_t i = 0; i < bar_width; i++) {
            ss << ((i <= fraction * bar_width) ? "█" : "―");
        }
        ss << "| ";
        ss << current_progress_percentage << " %";
        // ss << " [Estimated: " << (current_time / fraction) * (1 - fraction) << " s]";
        ss << " [Estimated: " << estimated_remaining_time << " s]";
        ss << " [Speed: " << current_speed << " Tick/s]";
        // ss << std::flush;
        // ss.flags(f);
        std::string output = ss.str();
        struct winsize size;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);
        size_t columns = size.ws_col;
        size_t output_lines = (output.size() + columns - 1)/ columns;
        // for(size_t i = 0; i < output_lines - 1; i++) {
        //     std::cout << "\033[F";
        // }
        std::cout << '\r' << output << std::flush;
    }
};