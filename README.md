### Torrent-Client

A lightweight C++ BitTorrent client implementation supporting downloads of single-file torrents.

## Features
- Single-file torrent downloads
- Multi-threaded peer connections
- Compact peer protocol support
- Progress tracking

## Dependencies
Required
- C++17 compatible compiler
- CMake (3.14 or higher)
- OpenSSL (for SHA-1 hashing)
- libcurl (for HTTP tracker communication)

Optional
- CPR (C++ Requests library) - will be automatically downloaded if not found

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
./torrent-client -d <output_directory> <torrent_file>
```

### Example

```bash
./torrent-client -d ./downloads ./resources/debian-9.3.0-ppc64el-netinst.torrent
```

## Main Components
- PieceStorage
Manages file pieces and disk storage
- TorrentClient
Main client class coordinating download process
- TorrentTracker
Handles communication with TCP trackers
- UdpTracker
Handles communication with UDP trackers
- PeerConnect
Manages individual peer connections
- TcpConnect
Handles TCP connections
- UdpClient
Handles UDP connections
- BencodeParser
Parses Bencode formatted data

## Limitations
- Supports only single-file torrents (no multi-file/directory structure)
- No seeding/upload capability
- No DHT support
- No magnet link support

## Features To Implement:
- Multi-file support
- DHT support
- Seeding

