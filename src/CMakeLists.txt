include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set(SOURCES
    main.cpp

    utils/BencodeParser.cpp
    utils/byte_tools.cpp

    core/TorrentFile.cpp
    core/TorrentTracker.cpp
    core/Piece.cpp
    core/PieceStorage.cpp
    core/TorrentClient.cpp
    core/UdpTracker.cpp
    core/TorrentTask.cpp
    net/TcpConnect.cpp
    net/PeerConnect.cpp
    net/Message.cpp
    net/UdpClient.cpp
)

add_executable(torrent-client ${SOURCES})

target_link_libraries(torrent-client
    OpenSSL::SSL
    OpenSSL::Crypto
    CURL::libcurl
    pthread
)

if(TARGET cpr)
    target_link_libraries(torrent-client cpr)
elseif(cpr_FOUND)
    target_link_libraries(torrent-client cpr::cpr)
endif()

target_include_directories(torrent-client
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(torrent-client PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
