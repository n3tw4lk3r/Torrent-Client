#include "ui/TorrentUI.hpp"
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

using namespace std::chrono_literals;

TorrentUI::TorrentUI(std::unique_ptr<TorrentClient> client)
    : client_(std::move(client))
{
    pause_button_ = ftxui::Button("", [this] { TogglePause(); });
    main_component_ = BuildUI();
}

TorrentUI::~TorrentUI() {
    running_ = false;
    if (update_thread_.joinable()) {
        update_thread_.join();
    }
}

void TorrentUI::TogglePause() {
    if (client_->IsPaused()) {
        client_->ResumeDownload();
    } else {
        client_->PauseDownload();
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
        
        if (event == Event::Character('p') || event == Event::Character('P') ||
            event == Event::Character(' ')) {
            TogglePause();
            return true;
        }
        
        return false;
    });
    
    return component;
}

ftxui::Element TorrentUI::Render() {
    using namespace ftxui;
    
    auto task = client_->GetCurrentTask();
    auto logs = client_->GetLogMessages(15);
    
    Elements header_elements = {
        text("Torrent Client") | bold | center,
    };
    
    auto header = vbox(header_elements);
    
    Elements task_info;
    
    Color status_color = Color::GrayLight;
    switch (task.status) {
        case TorrentStatus::kDownloading:
            status_color = Color::GreenLight;
            break;
        case TorrentStatus::kPaused:
            status_color = Color::YellowLight;
            break;
        case TorrentStatus::kCompleted:
            status_color = Color::BlueLight;
            break;
        case TorrentStatus::kError:
            status_color = Color::RedLight;
            break;
        default:
            status_color = Color::GrayLight;
    }
    
    std::string display_filename = task.filename;
    if (display_filename.length() > 50) {
        display_filename = display_filename.substr(0, 47) + "...";
    }
    
    task_info.push_back(hbox({
        text("File: ") | bold,
        text(display_filename)
    }));
    
    task_info.push_back(hbox({
        text("Status: ") | bold,
        text(task.GetStatusString()) | color(status_color)
    }));
    
    if (task.total_size > 0) {
        task_info.push_back(hbox({
            text("Size: ") | bold,
            text(task.GetFormattedSize())
        }));
    }
    
    task_info.push_back(hbox({
        text("Peers: ") | bold,
        text(task.GetPeersString())
    }));
    
    if (task.total_pieces_count > 0) {
        std::string progress_bar;
        int bar_width = 50;
        int filled = static_cast<int>((task.progress / 100.0) * bar_width);
        progress_bar = "[" + std::string(filled, '#') + 
                      std::string(bar_width - filled, ' ') + "] " +
                      task.GetFormattedProgress();
        
        task_info.push_back(text(" "));
        task_info.push_back(text(progress_bar) | color(Color::GreenLight) | center);
        task_info.push_back(text(" "));
        
        task_info.push_back(hbox({
            text("Progress: ") | bold,
            text(task.GetFormattedProgress()),
            text(" (" + std::to_string(task.downloaded_pieces_count) + 
                 "/" + std::to_string(task.total_pieces_count) + " pieces)")
        }) | center);
    }
    
    if (task.download_speed > 0) {
        task_info.push_back(hbox({
            text("Speed: ") | bold,
            text(task.GetFormattedSpeed())
        }) | center);
    }
    
    auto task_panel = window(text("Download Info"), 
        vbox(task_info) | frame
    );
    
    std::string button_text;
    Color button_color = Color::Default;
    
    if (task.status == TorrentStatus::kDownloading) {
        button_text = "⏸ Pause";
        button_color = Color::YellowLight;
    } else if (task.status == TorrentStatus::kPaused) {
        button_text = "▶ Resume";
        button_color = Color::GreenLight;
    } else if (task.status == TorrentStatus::kCompleted) {
        button_text = "✓ Done";
        button_color = Color::BlueLight;
    } else {
        button_text = "---";
    }
    
    auto button_element = 
        hbox({
            filler(),
            vbox({
                text(button_text) | bold | color(button_color) | center,
                text("Press P") | dim | center
            }) | size(HEIGHT, EQUAL, 3) | border,
            filler()
        });
    
    Elements log_entries;
    for (const auto& log : logs) {
        std::string log_text = log;
        if (log_text.length() > 80) {
            log_text = log_text.substr(0, 77) + "...";
        }
        
        auto log_element = text(" " + log_text);
        
        if (log.find("[ERROR]") != std::string::npos) {
            log_element = log_element | color(Color::RedLight);
        } else if (log.find("[WARNING]") != std::string::npos) {
            log_element = log_element | color(Color::YellowLight);
        } else if (log.find("[SYSTEM]") != std::string::npos) {
            log_element = log_element | color(Color::BlueLight);
        }
        
        log_entries.push_back(log_element);
    }
    
    auto log_panel = window(text("Activity Log"), 
        vbox(log_entries) | frame | flex
    );
    
    auto control_panel = window(text("Control Panel"),
        vbox({
            button_element,
            filler()
        }) | size(WIDTH, EQUAL, 25)
    );
    
    return vbox({
        header,
        separator(),
        hbox({
            task_panel | flex,
            separator(),
            control_panel
        }) | flex,
        separator(),
        log_panel | flex,
        separator(),
        text("Controls: P=Pause/Resume, Q=Quit") | center | dim
    });
}

void TorrentUI::UpdateUI() {
    while (running_) {
        std::this_thread::sleep_for(500ms);
    }
}

void TorrentUI::Run() {
    using namespace ftxui;
    
    auto screen = ScreenInteractive::Fullscreen();
    auto component = main_component_;
    
    std::atomic<bool> should_exit{false};
    
    component |= CatchEvent([&](Event event) {
        if (event == Event::Character('q') || event == Event::Character('Q') || 
            event == Event::Escape) {
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
    if (update_thread_.joinable()) {
        update_thread_.join();
    }
}
