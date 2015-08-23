set(ESMCONCAT_FILES
    esmconcat.cpp
)

add_executable(esmconcat
    ${ESMCONCAT_FILES}
)

target_link_libraries(esmconcat
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${Boost_FILESYSTEM_LIBRARY}
    components
)
