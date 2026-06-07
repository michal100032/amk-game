add_subdirectory(amcom)


add_executable(mniam_player main.c game/game.c amcom/amcom.c)
target_include_directories(mniam_player PRIVATE amcom/include game/include)
target_link_libraries(mniam_player PRIVATE am_amcom Ws2_32.lib)