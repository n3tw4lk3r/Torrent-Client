### Torrent-Client

A BitTorrent client written in C++ that supports downloading **single-file torrents** using both **TCP and UDP trackers**.  
The project features a **multi-threaded architecture** and a **text-based user interface (TUI)** built with FTXUI.

This project originally started as a university assignment and is now being refactored and improved.

## Screenshots
![Download In Progress](assets/images/downloading.png)
![Download completed](assets/images/finished.png)

## Features
- Single-file torrent downloads
- Multi-threaded peer connections
- TCP and UDP tracker support
- Compact peer protocol support
- Text User Interface (TUI)

## Dependencies
Required
- C++17 compatible compiler
- CMake (3.14 or higher)
- OpenSSL (for SHA-1 hashing)

Downloaded Automatically
- FTXUI - for TUI
- CPR - for network requests

## Dependencies Installation

### Ubuntu/Debian

```bash
sudo apt update
sudo apt install build-essential cmake libssl-dev libcurl4-openssl-dev
```

### Arch Linux
```bash
sudo pacman -S base-devel cmake openssl curl
```

### macOS
```bash
brew install cmake openssl curl
```

## Build
```bash
git clone https://github.com/n3tw4lk3r/Torrent-Client
cd Torrent-Client
mkdir build && cd build
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..
make -j$(nproc)

```
## Usage

```bash
# in Torrent-Client/build
# make sure you have output-directory created
src/simple-torrent-tui <torrent-file> <output-directory>
```

### Example

```bash
# in Torrent-Client/build
# downloads is created before executing the command
src/simple-torrent-tui ../resources/ubuntu-25.10-live-server-amd64.iso.torrent downloads
```

## Main Components

The project is split into several logical modules, each responsible for a distinct part of the BitTorrent protocol and application workflow.

### Core

- **TorrentClient**  
  The central orchestrator of the download process.  
  Coordinates trackers, peer connections, piece storage, and overall torrent state.

- **TorrentFile**  
  Represents parsed `.torrent` metadata, including piece hashes, announce URLs, and file information.

- **TorrentTracker**  
  Handles communication with HTTP/TCP trackers.

- **UdpTracker**  
  Implements the UDP tracker protocol (connect + announce).

- **PieceStorage**  
  Manages torrent pieces and blocks, tracks download progress, verifies piece hashes, and writes completed data to disk.

- **Piece**  
  Represents a single torrent piece split into blocks and tracks block-level download state.

### Networking

- **PeerConnection**  
  Manages communication with a single peer: handshake, bitfield exchange, piece requests, and message processing.

- **TCPConnection**  
  Low-level abstraction over TCP sockets used for peer communication.

- **UdpConnection**  
  Wrapper over UDP sockets, used primarily for tracker communication.

### Protocol & Utilities

- **Message**  
  Encapsulates BitTorrent peer protocol messages and provides parsing and serialization logic.

- **BencodeParser**  
  Parses Bencode-encoded data from strings and `.torrent` files.

## Limitations
- Supports only single-file torrents (no multi-file/directory structure)
- No seeding/upload capability
- No DHT support
- No magnet link support

## Planned Features And Fixes:
### Near Future
- Proper shutdown when pressing Q in TUI
- Fix duplicate "download finished" log messages

### Someday
- Multi-file torrent support
- Seeding/upload support

