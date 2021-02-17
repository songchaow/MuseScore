set(PARALLEL_EXE_NAME parallel)
set(PARALLEL_EXE_SRC main.cpp)

add_compile_definitions(QT_NO_DEBUG_OUTPUT)

add_executable(${PARALLEL_EXE_NAME} ${PARALLEL_EXE_SRC})
# target_link_libraries(${PARALLEL_EXE_NAME} ${REF_LIBS})
# target_include_directories(${PARALLEL_EXE_NAME} PUBLIC ${INCLUDES})

install(TARGETS ${PARALLEL_EXE_NAME} RUNTIME DESTINATION bin )

if(MSVC)
    # Force the "install" and "package" targets not to depend on the "all" target.
   set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY true)
   set(CMAKE_SKIP_PACKAGE_ALL_DEPENDENCY true)

   # Set the debugging properties for the "mscore" project.
   file(TO_NATIVE_PATH "${CMAKE_INSTALL_PREFIX}/bin" VS_DEBUGGER_WORKING_DIRECTORY)
   if (NOT ${CMAKE_VERSION} VERSION_LESS "3.12.0")
      set_target_properties(${PARALLEL_EXE_NAME} PROPERTIES VS_DEBUGGER_COMMAND "${VS_DEBUGGER_WORKING_DIRECTORY}\\${PARALLEL_EXE_NAME}${CMAKE_EXECUTABLE_SUFFIX}")
   endif ()
   if (NOT ${CMAKE_VERSION} VERSION_LESS "3.8.0")
      set_target_properties(${PARALLEL_EXE_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${VS_DEBUGGER_WORKING_DIRECTORY}")
   endif ()
   if (NOT ${CMAKE_VERSION} VERSION_LESS "3.13.0")
      set_target_properties(${PARALLEL_EXE_NAME} PROPERTIES VS_DEBUGGER_COMMAND_ARGUMENTS "--debug")
   endif ()
endif(MSVC)