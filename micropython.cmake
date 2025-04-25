# Create an INTERFACE library for our C module.
add_library(usermod_eigenmath INTERFACE)

# Add our source files to the lib
target_sources(usermod_eigenmath INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/eigenmath.c
    ${CMAKE_CURRENT_LIST_DIR}/eheap.c
    ${CMAKE_CURRENT_LIST_DIR}/eigenmath_mpy.c
)

# Add the current directory as an include directory.
target_include_directories(usermod_eigenmath INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

# Link our INTERFACE library to the usermod target.
target_link_libraries(usermod INTERFACE usermod_eigenmath)

