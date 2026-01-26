#include "ui/TorrentUi.hpp"

#include <chrono>
#include <iomanip>
#include <sstream>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

using namespace std::chrono_literals;

TorrentUi::TorrentUi(std::unique_ptr<TorrentClient> client) :
    client_(std::move(client))
{
    main_component_ = BuildUi();
    start_time_ = std::chrono::steady_clock::now();
}

TorrentUi::~TorrentUi() {
    running_ = false;
    if (update_thread_.joinable()) {
        update_thread_.join();
    }
}

ftxui::Component TorrentUi::BuildUi() {
    using namespace ftxui;
    
    auto renderer = Renderer([this] {
        return Render();
    });
    
    auto component = CatchEvent(renderer, [this](Event event) {
        if (event == Event::Character('q') || event == Event::Character('Q') || 
            event == Event::Escape) {
            return false;
        }
        
        return false;
    });
    
    return component;
}

std::string FormatDuration(std::chrono::seconds duration) {
    auto hours = std::chrono::duration_cast<std::chrono::hours>(duration);
    duration -= hours;
    auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration);
    duration -= minutes;
    auto seconds = duration;
    
    std::ostringstream oss;
    if (hours.count() > 0) {
        oss << hours.count() << "h ";
    }
    if (minutes.count() > 0 || hours.count() > 0) {
        oss << minutes.count() << "m ";
    }
    oss << seconds.count() << "s";
    return oss.str();
}

std::string FormatRemainingTime(uint64_t downloaded, uint64_t total_size, 
                                uint64_t download_speed) {
    if (download_speed == 0 || downloaded >= total_size) {
        return "--:--";
    }
    
    uint64_t remaining_bytes = total_size - downloaded;
    double remaining_seconds = static_cast<double>(remaining_bytes) / download_speed;
    
    auto hours = static_cast<int>(remaining_seconds / 3600);
    auto minutes = static_cast<int>((remaining_seconds - hours * 3600) / 60);
    auto seconds = static_cast<int>(remaining_seconds) % 60;
    
    std::ostringstream oss;
    if (hours > 0) {
        oss << std::setfill('0') << std::setw(2) << hours << ":";
    }
    oss << std::setfill('0') << std::setw(2) << minutes << ":"
        << std::setfill('0') << std::setw(2) << seconds;
    return oss.str();
}

ftxui::Element TorrentUi::Render() {
    using namespace ftxui;
    
    auto task = client_->GetCurrentTask();
    auto logs = client_->GetLogMessages(30);
    
    auto header = hbox({
        text(" TORRENT CLIENT ") | bold | inverted | center
    }) | center | border;
    
    Color status_color = Color::GrayLight;
    
    switch (task.status) {
        case TorrentStatus::kDownloading:
            status_color = Color::GreenLight;
            break;
        case TorrentStatus::kCompleted:
            status_color = Color::CyanLight;
            break;
        case TorrentStatus::kError:
            status_color = Color::RedLight;
            break;
        case TorrentStatus::kStopped:
            status_color = Color::YellowLight;
            break;
        default:
            status_color = Color::GrayLight;
    }
    
    Elements task_info;
    
    std::string display_filename = task.filename;
    if (display_filename.length() > 60) {
        display_filename = display_filename.substr(0, 57) + "...";
    }
    
    task_info.push_back(hbox({
        filler(),
        text("File: ") | bold,
        text(display_filename),
        filler()
    }));
    
    task_info.push_back(hbox({
        filler(),
        text("Status: ") | bold,
        text(task.GetStatusString()) | bold | color(status_color),
        filler()
    }));
    
    if (task.total_size > 0) {
        task_info.push_back(hbox({
            filler(), 
            text("Size: ") | bold,
            text(task.GetFormattedSize()),
            filler()
        }));
        
        task_info.push_back(hbox({
            filler(),
            text("Downloaded: ") | bold,
            text(task.GetFormattedDownloaded() + " / " + task.GetFormattedSize()),
            filler()
        }));
    }
    
    task_info.push_back(hbox({
        filler(),
        text("Peers: ") | bold,
        text(task.GetPeersString()),
        filler()
    }));
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);
    task_info.push_back(hbox({
        filler(),
        text("Time elapsed: ") | bold,
        text(FormatDuration(elapsed)),
        filler(),
    }));
    
    task_info.push_back(text(""));
    
    const int kBarWidth = 50;
    int filled = static_cast<int>((task.progress / 100.0) * kBarWidth);
    int percentage = static_cast<int>(task.progress);
    
    std::string progress_bar;
    for (int i = 0; i < kBarWidth; ++i) {
        if (i < filled) {
            progress_bar += "#";
        } else {
            progress_bar += ".";
        }
    }
    
    auto progress_element = vbox({
        hbox({
            text("["),
            text(progress_bar) | color(Color::GreenLight),
            text("] "),
            text(std::to_string(percentage) + "%") | bold
        }) | center,
        hbox({
            filler(),
            text("Pieces: " + std::to_string(task.downloaded_pieces_count) + 
                 "/" + std::to_string(task.total_pieces_count)) | dim,
            filler()
        })
    });
    
    task_info.push_back(progress_element);
    
    auto info_panel = window(
        text(" DOWNLOAD INFO ") | bold | center,
        vbox(task_info) | frame | size(HEIGHT, LESS_THAN, 20)
    );
    
    Elements log_entries;
    for (const auto& log : logs) {
        std::string log_text = log;
        if (log_text.length() > 90) {
            log_text = log_text.substr(0, 87) + "...";
        }
        
        Color log_color = Color::GrayLight;
        
        auto log_element = hbox({
            text(log_text) | color(log_color)
        });
        
        log_entries.push_back(log_element);
    }
    
    auto log_panel = window(
        text(" ACTIVITY LOG ") | bold | center,
        vbox(log_entries) | frame | flex
    );
    
    auto footer = hbox({
        text(" Press "),
        text(" Q ") | bold | inverted,
        text(" to quit ")
    }) | center | dim;
    
    return vbox({
        header,
        separator(),
        info_panel,
        separator(),
        text("") | size(HEIGHT, EQUAL, 1),
        log_panel | flex,
        separator(),
        footer
    });
}

void TorrentUi::Run() {
    using namespace ftxui;
    
    auto screen = ScreenInteractive::Fullscreen();
    auto component = main_component_;
    
    std::atomic<bool> should_exit{false};
    
    component = component | ftxui::CatchEvent([&](ftxui::Event event) {
        if (event == ftxui::Event::Character('q') ||
            event == ftxui::Event::Character('Q') ||
            event == ftxui::Event::Escape) {
            should_exit = true;
            screen.Exit();
            return true;
        }
        return false;
    });
    
    update_thread_ = std::thread([&, &screen = screen]() {
        while (!should_exit && running_) {
            std::this_thread::sleep_for(500ms);
            screen.PostEvent(Event::Custom);
        }
    });
    
    screen.Loop(component);
    
    running_ = false;
    should_exit = true;
    if (update_thread_.joinable()) {
        update_thread_.join();
    }
}
