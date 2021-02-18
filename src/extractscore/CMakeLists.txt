set(EXTRACT_EXE_NAME extractscore)
set(EXTRACT_EXE_SRC main.cpp utils.cpp utils.h)
set(SINGLE_EXE_NAME single)
set(SINGLE_EXE_SRC single.cpp utils.cpp utils.h)

add_compile_definitions(QT_NO_DEBUG_OUTPUT NDEBUG)
# protobuf generation
set(PROTOBUF_OUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
file(GLOB PROTOBUF_DEFINITION_FILES "*.proto")
foreach(file ${PROTOBUF_DEFINITION_FILES})
    set(PROTOBUF_CMD "protoc" "--proto_path=${CMAKE_CURRENT_LIST_DIR}" "--cpp_out=${PROTOBUF_OUT_DIRECTORY}" "--python_out=${PROTOBUF_OUT_DIRECTORY}" "${file}")
    execute_process(COMMAND ${PROTOBUF_CMD}
            WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
            RESULT_VARIABLE PROTOBUF_RESULT
            OUTPUT_VARIABLE PROTOBUF_OUTPUT_VARIABLE
            ERROR_VARIABLE PROTOBUF_ERR_VARIABLE)
endforeach()
file(GLOB PROTOBUF_MODELS_INCLUDES ${PROTOBUF_OUT_DIRECTORY}/*.pb.cc ${PROTOBUF_OUT_DIRECTORY}/*.pb.h)


# set(PROTOBUF_MODELS_INCLUDES sequence.pb.cc)
# set(SERIALIZE_HEADER sequence.pb.h)
set(PROTOBUF_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/thirdparty/protobuf/src ${PROTOBUF_OUT_DIRECTORY})

set(REF_LIBS
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
    libprotobuf
    )
set(INCLUDES
    ${PROJECT_SOURCE_DIR}/src/libmscore
    ${PROJECT_SOURCE_DIR}/src/notation
    ${PROJECT_SOURCE_DIR}/src/framework
    ${PROJECT_SOURCE_DIR}/src/framework/global
    ${PROTOBUF_INCLUDE_DIR}
)

# extract

add_executable(${EXTRACT_EXE_NAME} ${EXTRACT_EXE_SRC} ${PROTOBUF_MODELS_INCLUDES})
set_target_properties(${EXTRACT_EXE_NAME} PROPERTIES COMPILE_FLAGS "-fPIC")
target_link_libraries(${EXTRACT_EXE_NAME} ${REF_LIBS})
target_include_directories(${EXTRACT_EXE_NAME} PUBLIC ${INCLUDES})

install(TARGETS ${EXTRACT_EXE_NAME} RUNTIME DESTINATION bin )


if(MSVC)
    # Force the "install" and "package" targets not to depend on the "all" target.
   set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY true)
   set(CMAKE_SKIP_PACKAGE_ALL_DEPENDENCY true)

   # Set the debugging properties for the "mscore" project.
   file(TO_NATIVE_PATH "${CMAKE_INSTALL_PREFIX}/bin" VS_DEBUGGER_WORKING_DIRECTORY)
   if (NOT ${CMAKE_VERSION} VERSION_LESS "3.12.0")
      set_target_properties(${EXTRACT_EXE_NAME} PROPERTIES VS_DEBUGGER_COMMAND "${VS_DEBUGGER_WORKING_DIRECTORY}\\${EXTRACT_EXE_NAME}${CMAKE_EXECUTABLE_SUFFIX}")
   endif ()
   if (NOT ${CMAKE_VERSION} VERSION_LESS "3.8.0")
      set_target_properties(${EXTRACT_EXE_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${VS_DEBUGGER_WORKING_DIRECTORY}")
   endif ()
   if (NOT ${CMAKE_VERSION} VERSION_LESS "3.13.0")
      set_target_properties(${EXTRACT_EXE_NAME} PROPERTIES VS_DEBUGGER_COMMAND_ARGUMENTS "--debug")
   endif ()
endif(MSVC)

# single

add_executable(${SINGLE_EXE_NAME} ${SINGLE_EXE_SRC} ${PROTOBUF_MODELS_INCLUDES})
set_target_properties(${SINGLE_EXE_NAME} PROPERTIES COMPILE_FLAGS "-fPIC")
target_link_libraries(${SINGLE_EXE_NAME} ${REF_LIBS})
target_include_directories(${SINGLE_EXE_NAME} PUBLIC ${INCLUDES})

install(TARGETS ${SINGLE_EXE_NAME} RUNTIME DESTINATION bin )

if(MSVC)
    # Force the "install" and "package" targets not to depend on the "all" target.
   set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY true)
   set(CMAKE_SKIP_PACKAGE_ALL_DEPENDENCY true)

   # Set the debugging properties for the "mscore" project.
   file(TO_NATIVE_PATH "${CMAKE_INSTALL_PREFIX}/bin" VS_DEBUGGER_WORKING_DIRECTORY)
   if (NOT ${CMAKE_VERSION} VERSION_LESS "3.12.0")
      set_target_properties(${SINGLE_EXE_NAME} PROPERTIES VS_DEBUGGER_COMMAND "${VS_DEBUGGER_WORKING_DIRECTORY}\\${SINGLE_EXE_NAME}${CMAKE_EXECUTABLE_SUFFIX}")
   endif ()
   if (NOT ${CMAKE_VERSION} VERSION_LESS "3.8.0")
      set_target_properties(${SINGLE_EXE_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${VS_DEBUGGER_WORKING_DIRECTORY}")
   endif ()
   if (NOT ${CMAKE_VERSION} VERSION_LESS "3.13.0")
      set_target_properties(${SINGLE_EXE_NAME} PROPERTIES VS_DEBUGGER_COMMAND_ARGUMENTS "--debug")
   endif ()
endif(MSVC)

# parallel
set(PARALLEL_PROJ_SRC parallel.cpp utils.cpp utils.h)

add_executable(parallel ${PARALLEL_PROJ_SRC})
if (UNIX)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    target_link_libraries(parallel PRIVATE Threads::Threads)
endif (UNIX)
install(TARGETS parallel RUNTIME DESTINATION bin )