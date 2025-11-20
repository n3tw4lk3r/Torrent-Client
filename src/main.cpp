#include "core/TorrentClient.hpp"
#include <iostream>
#include <filesystem>
#include <cstring>
#include <string>

void PrintUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " -d <output_directory> <torrent_file>" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -d <directory>   Output directory for downloaded file" << std::endl;
    std::cout << "  -h, --help       Show this help message" << std::endl;
}

int main(int argc, char *argv[]) {
    std::string output_directory;
    std::string torrent_file;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-d" && i + 1 < argc) {
            output_directory = argv[++i];
        }
        else if (arg == "-h" || arg == "--help") {
            PrintUsage(argv[0]);
            return 0;
        }
        else if (arg[0] != '-') {
            torrent_file = arg;
        }
        else {
            std::cerr << "Unknown option: " << arg << std::endl;
            PrintUsage(argv[0]);
            return 1;
        }
    }

    if (torrent_file.empty()) {
        std::cerr << "Error: No torrent file specified" << std::endl;
        PrintUsage(argv[0]);
        return 1;
    }

    if (output_directory.empty()) {
        std::cerr << "Error: No output directory specified" << std::endl;
        PrintUsage(argv[0]);
        return 1;
    }

    if (!std::filesystem::exists(torrent_file)) {
        std::cerr << "Error: Torrent file not found: " << torrent_file << std::endl;
        return 1;
    }

    if (!std::filesystem::exists(output_directory)) {
        std::cerr << "Error: Output directory not found: " << output_directory << std::endl;
        return 1;
    }

    try {
        std::cout << "Starting torrent download..." << std::endl;
        std::cout << "Torrent file: " << torrent_file << std::endl;
        std::cout << "Output directory: " << output_directory << std::endl;

        TorrentClient client;
        client.DownloadTorrent(torrent_file, output_directory);

        std::cout << "Download completed successfully!" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
