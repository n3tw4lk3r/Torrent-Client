#include "ui/TorrentUI.hpp"
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

using namespace std::chrono_literals;

TorrentUI::TorrentUI(std::unique_ptr<TorrentClient> client)
    : client_(std::move(client)) {
    main_component_ = BuildUI();
}

TorrentUI::~TorrentUI() {
    running_ = false;
    if (update_thread_.joinable()) {
        update_thread_.join();
    }
}

ftxui::Component TorrentUI::BuildUI() {
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

ftxui::Element TorrentUI::Render() {
    using namespace ftxui;
    
    auto task = client_->GetCurrentTask();
    auto logs = client_->GetLogMessages(30);
    
    auto header = hbox({
        text(" TORRENT CLIENT ") | bold | inverted | center
    }) | center | border;
    
    Color status_color = Color::GrayLight;
    std::string status_symbol = "  ";
    
    switch (task.status) {
        case TorrentStatus::kDownloading:
            status_color = Color::GreenLight;
            status_symbol = ">>";
            break;
        case TorrentStatus::kCompleted:
            status_color = Color::CyanLight;
            status_symbol = "OK";
            break;
        case TorrentStatus::kError:
            status_color = Color::RedLight;
            status_symbol = "!!";
            break;
        default:
            status_color = Color::GrayLight;
            status_symbol = "--";
    }
    
    Elements task_info;
    
    std::string display_filename = task.filename;
    if (display_filename.length() > 60) {
        display_filename = display_filename.substr(0, 57) + "...";
    }
    
    task_info.push_back(hbox({
        text("File:    ") | bold,
        text(display_filename)
    }));
    
    task_info.push_back(hbox({
        text("Status:  ") | bold,
        text("[" + status_symbol + "] " + task.GetStatusString()) | bold | color(status_color)
    }));
    
    if (task.total_size > 0) {
        task_info.push_back(hbox({
            text("Size:    ") | bold,
            text(task.GetFormattedSize())
        }));
    }
    
    task_info.push_back(hbox({
        text("Peers:   ") | bold,
        text(task.GetPeersString())
    }));
    
    if (task.total_pieces_count > 0 && task.status == TorrentStatus::kDownloading) {
        task_info.push_back(text(""));
        
        int bar_width = 50;
        int filled = static_cast<int>((task.progress / 100.0) * bar_width);
        int percentage = static_cast<int>(task.progress);
        
        std::string progress_bar;
        for (int i = 0; i < bar_width; i++) {
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
        
    }
    
    auto info_panel = window(
        text(" DOWNLOAD INFO ") | bold | center,
        vbox(task_info) | frame | size(HEIGHT, LESS_THAN, 12)
    );
    
    Elements log_entries;
    for (const auto& log : logs) {
        std::string log_text = log;
        if (log_text.length() > 90) {
            log_text = log_text.substr(0, 87) + "...";
        }
        
        std::string log_prefix = " ";
        Color log_color = Color::GrayLight;
        
        if (log.find("[ERROR]") != std::string::npos) {
            log_prefix = "! ";
            log_color = Color::RedLight;
        } else if (log.find("[WARNING]") != std::string::npos) {
            log_prefix = "* ";
            log_color = Color::YellowLight;
        } else if (log.find("[SYSTEM]") != std::string::npos) {
            log_prefix = "> ";
            log_color = Color::BlueLight;
        } else if (log.find("[INFO]") != std::string::npos) {
            log_prefix = "- ";
            log_color = Color::GreenLight;
        }
        
        auto log_element = hbox({
            text(log_prefix) | color(log_color),
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

void TorrentUI::Run() {
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
