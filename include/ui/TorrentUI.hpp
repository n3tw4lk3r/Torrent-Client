#pragma once

#include <atomic>
#include <memory>
#include <thread>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include "core/TorrentClient.hpp"

class TorrentUI {
public:
    TorrentUI(std::unique_ptr<TorrentClient> client);
    ~TorrentUI();
    
    void Run();

private:
    std::unique_ptr<TorrentClient> client_;
    std::atomic<bool> running_{true};
    std::thread update_thread_;
    
    ftxui::Component pause_button_;
    ftxui::Component main_component_;
    
    void TogglePause();
    void UpdateUI();
    ftxui::Component BuildUI();
    ftxui::Element Render();
};
