project(beauty_example)

add_executable(beauty_example
	pc/main.cpp
	pc/file_handler.cpp
	pc/file_storage_handler.cpp
	${Beauty_Sources}
)

include_directories(
    ${CMAKE_SOURCE_DIR}/examples/pc
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(beauty_example PRIVATE asio::asio)
