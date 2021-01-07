set(EXECUTABLE_NAME extractscore)
set(EXECUTABLE_SRC main.cpp)

add_executable(${EXECUTABLE_NAME} ${EXECUTABLE_SRC})
target_link_libraries(${EXECUTABLE_NAME}
    libmscore
    notation
    actions
    appshell
    context
    shortcuts
    workspace
    audio
    midi
    midi_old
    userscores
    notation
    commonscene
    palette
    inspector
    playback
    instruments
    converter
    fonts
    )
target_include_directories(${EXECUTABLE_NAME} PUBLIC 
    ${PROJECT_SOURCE_DIR}/src/libmscore
    ${PROJECT_SOURCE_DIR}/src/notation
    ${PROJECT_SOURCE_DIR}/src/framework
    ${PROJECT_SOURCE_DIR}/src/framework/global
    )

if (OS_IS_WIN)
    install(TARGETS ${EXECUTABLE_NAME} RUNTIME DESTINATION bin )
endif (OS_IS_WIN)

if(MSVC)
    # Force the "install" and "package" targets not to depend on the "all" target.
   set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY true)
   set(CMAKE_SKIP_PACKAGE_ALL_DEPENDENCY true)

   # Set the debugging properties for the "mscore" project.
   file(TO_NATIVE_PATH "${CMAKE_INSTALL_PREFIX}/bin" VS_DEBUGGER_WORKING_DIRECTORY)
   if (NOT ${CMAKE_VERSION} VERSION_LESS "3.12.0")
      set_target_properties(${EXECUTABLE_NAME} PROPERTIES VS_DEBUGGER_COMMAND "${VS_DEBUGGER_WORKING_DIRECTORY}\\${EXECUTABLE_NAME}${CMAKE_EXECUTABLE_SUFFIX}")
   endif ()
   if (NOT ${CMAKE_VERSION} VERSION_LESS "3.8.0")
      set_target_properties(${EXECUTABLE_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${VS_DEBUGGER_WORKING_DIRECTORY}")
   endif ()
   if (NOT ${CMAKE_VERSION} VERSION_LESS "3.13.0")
      set_target_properties(${EXECUTABLE_NAME} PROPERTIES VS_DEBUGGER_COMMAND_ARGUMENTS "--debug")
   endif ()
endif(MSVC)