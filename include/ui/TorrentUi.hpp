#pragma once

#include <atomic>
#include <memory>
#include <thread>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include "core/TorrentClient.hpp"

class TorrentUi {
public:
    TorrentUi(std::unique_ptr<TorrentClient> client);
    ~TorrentUi();
    
    void Run();

private:
    std::unique_ptr<TorrentClient> client_;
    std::atomic<bool> running_{true};
    std::thread update_thread_;
    
    ftxui::Component main_component_;
    
    ftxui::Component BuildUi();
    ftxui::Element Render();
};
