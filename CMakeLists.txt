cmake_minimum_required(VERSION 3.16.3)

project(bp_dill)

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s")

include_directories(include local/include
                    models
                    deps/dlg/include)
link_directories(local/lib)

set(SRCS src/bp.c src/io_util.c src/logging.c deps/dlg/src/dlg/dlg.c)
set(LIBS libdill.a)

add_compile_options(-Wall -fPIC)
add_compile_definitions(DLG_LOG_LEVEL=dlg_level_info)

add_executable(waterflow src/waterflow.c ${SRCS})
target_link_libraries(waterflow ${LIBS})

add_executable(pingpong src/pingpong.c ${SRCS})
target_link_libraries(pingpong ${LIBS})

add_executable(button_blink src/button_blink.c ${SRCS})
target_link_libraries(button_blink ${LIBS})

add_executable(pelican src/pelican.c ${SRCS})
target_link_libraries(pelican ${LIBS})

