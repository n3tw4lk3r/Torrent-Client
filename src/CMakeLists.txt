add_library(core STATIC
    core/Piece.cpp
    core/PieceStorage.cpp
    core/TorrentClient.cpp
    core/TorrentFile.cpp
    core/TorrentTracker.cpp
    core/UdpTracker.cpp
    core/TorrentTask.cpp
    utils/BencodeParser.cpp
    utils/byte_tools.cpp
    net/Message.cpp
    net/PeerConnect.cpp
    net/TcpConnect.cpp
    net/UdpClient.cpp
)

target_include_directories(core PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_include_directories(core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(core PUBLIC
    OpenSSL::SSL
    OpenSSL::Crypto
    CURL::libcurl
    cpr::cpr
)

add_executable(simple-torrent-console
    main.cpp
)

target_link_libraries(simple-torrent-console
    core
)

add_executable(simple-torrent-tui
    main_tui.cpp
    ui/TorrentUI.cpp
)

target_include_directories(simple-torrent-tui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(simple-torrent-tui
    core
    ftxui::component
)

install(TARGETS simple-torrent-console simple-torrent-tui
    DESTINATION bin
)
