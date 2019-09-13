#include <thread>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <utility>
#include <cmath>
#include <algorithm>
#include <sys/ioctl.h>
#include <unistd.h>

#include "windowhistory.hpp"

enum class DisplayComponent {
    None,
    ElapsedTime,
    RawProgress,
    ProgressBar,
    Percentage,
    EstimatedTime,
    Speed,
    All
};

struct PrinterBase {};

template<DisplayComponent dc>
struct Printer;

template<typename T, size_t N, DisplayComponent... args>
class ProgressBar {
public:
    ProgressBar(const T& progress, const T& max_progress) 
        : progress(progress), max_progress(max_progress), speed_history() {
            for(size_t i = 0; i < N; i++) {
                pbs[i] = nullptr;
            }
        }

    void init() {
        start = std::chrono::steady_clock::now();
        history.emplace(start, progress);
        while(true) {
            print_progressbar();
            std::this_thread::sleep_for(std::chrono::milliseconds(sampling_freq_millis));
            if(progress == max_progress) { 
                print_progressbar();
                plot_speed_history();
                break;
            }
        }
        time_point end = std::chrono::steady_clock::now();
        // std::cout << "\nJob completed in " << duration_sec(start, end) << " s" << std::endl;
    }
    
    ProgressBar& addComponent(const DisplayComponent& displayComponent) {
        displayComponents.push_back(displayComponent);
        return *this;
    }

private:
    template<DisplayComponent dc> friend struct Printer;
    typedef std::chrono::time_point<std::chrono::steady_clock> time_point;
    /* configurations */
    const size_t bar_width = 50;
    const size_t sampling_freq_millis = 100;
    static constexpr const char* fillchar = "█";
    static constexpr const char* emptychar = "―";
    static constexpr const size_t history_window_size = 20;
    static constexpr const size_t speed_history_size = 20;
    static constexpr const size_t speed_history_plot_height = 15;
    /* end configurations */

    /* members */
    const T& progress;
    const T& max_progress;
    WindowHistory<std::pair<time_point, T>, history_window_size> history;
    std::array<double, speed_history_size> speed_history;
    double current_speed;
    time_point start;
    time_point current;
    std::vector<DisplayComponent> displayComponents;
    static constexpr const DisplayComponent dcs[N] = {args...};
    static constexpr PrinterBase* pbs[N] = {};
    /* end members */
    
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

    static std::string fmt_time(double tot_seconds) {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2);
        ss << std::setw(2);
        size_t days = tot_seconds / (60 * 60 * 24);
        size_t hours = std::fmod(tot_seconds,  60 * 60 * 24) / (60 * 60);
        size_t minutes = std::fmod(tot_seconds, 60 * 60) / 60;
        double seconds = std::fmod(tot_seconds, 60);
        bool started = false;
        if(days > 0) { ss << days << 'd' << std::setfill('0'); started = true;}
        if(started || hours > 0) { ss << hours << 'h' << std::setfill('0'); started = true; }
        if(started || minutes > 0) { ss << minutes << 'm' << std::setfill('0'); }
        ss << seconds << 's';
        std::string out = ss.str();
        if(out.size() > 15) { out = "-"; }
        return out;
    }

    void print_progressbar() {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2);
        current = std::chrono::steady_clock::now();
        history.emplace(current, progress);
        current_speed = calculate_speed();
        speed_history[map_progress(speed_history_size)] = current_speed;
        for(const auto& displayComponent : dcs) {
            Printer<displayComponent>::print(ss, this);
        }

        // double current_time = duration_sec(start, current);
        // double estimated_remaining_time = (max_progress - progress) / current_speed;
        // double current_progress_percentage = (progress * 100.0) / max_progress;
        // ss << "[Elapsed: " << fmt_time(current_time) << "] ";
        // ss << "[Progress: " << progress << '/' << max_progress << " Ticks] ";
        // ss << "|";
        // for(size_t i = 0; i < bar_width; i++) {
        //     ss << ((i <= (progress * bar_width) / max_progress) ? fillchar : emptychar);
        // }
        // ss << "| ";
        // ss << current_progress_percentage << " %";
        // ss << " [Est.Remaining: " << fmt_time(estimated_remaining_time) << "]";
        // ss << " [Speed: " << current_speed << " Tick/s]";
        std::string output = ss.str();
        struct winsize size;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);
        size_t columns = size.ws_col;
        size_t output_lines = (output.size() + columns - 1)/ columns;
        // for(size_t i = 0; i < output_lines - 1; i++) {
        //     std::cout << "\033[F";
        // }
        std::cout << "\33[2K\r" << output << std::flush;
    }

    void print_elapsedtime(std::ostringstream& ss) const {
        double current_time = duration_sec(start, current);
        ss << "[Elapsed: " << fmt_time(current_time) << "]";
    }

    void print_rawprogress(std::ostringstream& ss) const {
        ss << "[Progress: " << progress << '/' << max_progress << " Ticks]";
    }

    void print_progressbar(std::ostringstream& ss) const {
        ss << "|";
        for(size_t i = 0; i < bar_width; i++) {
            ss << ((i <= (progress * bar_width) / max_progress) ? fillchar : emptychar);
        }
        ss << "|";
    }

    void print_percentage(std::ostringstream& ss) const {
        double current_progress_percentage = (progress * 100.0) / max_progress;
        ss << current_progress_percentage << " %";
    }

    void print_estimatedtime(std::ostringstream& ss) const {
        double estimated_remaining_time = (max_progress - progress) / current_speed;
        ss << " [Est.Remaining: " << fmt_time(estimated_remaining_time) << "]";
    }

    void print_speed(std::ostringstream& ss) const {
        ss << " [Speed: " << current_speed << " Tick/s]";
    }

    void plot_speed_history() const {
        std::array<std::array<std::string, speed_history_size>, speed_history_plot_height> plot;
        for(auto& s: plot) for(auto& c: s) c = "·";
        double max_speed = 0;
        for(const auto& speed: speed_history) { 
            max_speed = (max_speed < speed && speed != std::numeric_limits<double>::infinity()) ? speed : max_speed;
        }
        for(size_t i = 0; i < speed_history_size; i++) {
            if(speed_history[i] != std::numeric_limits<double>::infinity()) {
                plot[(size_t)((speed_history[i] / max_speed) * (speed_history_plot_height - 1))][i] = "⏺";
            }
        }
        std::cout << '\n';
        for(size_t i = 0; i < speed_history_plot_height; i++) {
            std::cout << "│ ";
            for(size_t j = 0; j < speed_history_size; j++) {
                std::cout << plot[speed_history_plot_height - 1 - i][j] << "  ";
            }
            std::cout << '\n';
        }
        std::cout << "└─";
        for(size_t i = 0; i < speed_history_size; i++) {
            std::cout << "───";
        }
        std::cout << "\n";
        for(size_t i = 1; i <= speed_history_size; i++) {
            std::cout << std::setw(3) << ((i * 100) / speed_history_size) << std::flush;
        }
        std::cout << std::endl;
    }
};

template<>
struct Printer<DisplayComponent::ElapsedTime> : PrinterBase {
    template<typename T, size_t N, DisplayComponent... args>
    static void print(std::ostringstream& ss, const ProgressBar<T, N, args...>* pb) {
        double current_time = duration_sec(pb->start, pb->current);
        ss << "[Elapsed: " << ProgressBar<T, N, args...>::fmt_time(current_time) << "]";
    }
};

template<>
struct Printer<DisplayComponent::All> : PrinterBase {
    template<typename T, size_t N, DisplayComponent... args>
    static void print(std::ostringstream& ss, const ProgressBar<T, N, args...>* pb) {
        Printer<DisplayComponent::ElapsedTime>::print(ss, pb);
        Printer<DisplayComponent::ElapsedTime>::print(ss, pb);
    }
};