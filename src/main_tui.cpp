#include "ui/TorrentUI.hpp"
#include "core/TorrentClient.hpp"
#include <iostream>
#include <filesystem>
#include <atomic>
#include <thread>

std::atomic<bool> is_download_completed = false;

void download_thread_function(TorrentClient* client, 
                              const std::filesystem::path& torrent_file_path,
                              const std::filesystem::path& output_directory) {
    try {
        client->DownloadTorrent(torrent_file_path, output_directory);
        is_download_completed = true;
    } catch (const std::exception& e) {
        std::cerr << "Download error: " << e.what() << std::endl;
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " <torrent-file> <output-directory>" << std::endl;
        return 1;
    }
    
    std::filesystem::path torrent_file_path = argv[1];
    std::filesystem::path output_directory = argv[2];
    
    if (!std::filesystem::exists(torrent_file_path)) {
        std::cerr << "Error: Torrent file not found: " << torrent_file_path << std::endl;
        return 1;
    }
    
    if (!std::filesystem::exists(output_directory)) {
        std::filesystem::create_directories(output_directory);
    }
    
    try {
        auto *client = new TorrentClient();
        
        std::thread download_thread(download_thread_function, client, torrent_file_path, output_directory);
        
        auto client_ptr = std::unique_ptr<TorrentClient>(client);
        TorrentUI torrent_ui(std::move(client_ptr));
        
        torrent_ui.Run();
        
        if (download_thread.joinable()) {
            download_thread.join();
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
