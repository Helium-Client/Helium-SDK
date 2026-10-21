cmake_minimum_required(VERSION 3.20)
project(HeliumSDKV1 LANGUAGES C CXX)

include(CTest)

set(DINIT_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Dinit/include")
if(NOT EXISTS "${DINIT_INCLUDE_DIR}/dinit/dinit.h")
    message(FATAL_ERROR "Dinit must be checked out as a sibling directory at ../Dinit")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(helium_cpp_example SHARED examples/cpp-minimal/plugin.cpp)
target_include_directories(helium_cpp_example PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include" "${DINIT_INCLUDE_DIR}")
target_compile_definitions(helium_cpp_example PRIVATE HELIUM_PLUGIN_BUILD=1)
set_target_properties(helium_cpp_example PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
    POSITION_INDEPENDENT_CODE YES
)

set(HELIUM_PACKAGE_DIR "${CMAKE_BINARY_DIR}/package/dev.helium.cpp-example")
file(MAKE_DIRECTORY "${HELIUM_PACKAGE_DIR}")
set_target_properties(helium_cpp_example PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${HELIUM_PACKAGE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${HELIUM_PACKAGE_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY "${HELIUM_PACKAGE_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${HELIUM_PACKAGE_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${HELIUM_PACKAGE_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${HELIUM_PACKAGE_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${HELIUM_PACKAGE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${HELIUM_PACKAGE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${HELIUM_PACKAGE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${HELIUM_PACKAGE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${HELIUM_PACKAGE_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${HELIUM_PACKAGE_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${HELIUM_PACKAGE_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO "${HELIUM_PACKAGE_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL "${HELIUM_PACKAGE_DIR}"
)
file(GENERATE
    OUTPUT "${HELIUM_PACKAGE_DIR}/helium-plugin.json"
    CONTENT "{\n  \"id\": \"dev.helium.cpp-example\",\n  \"name\": \"Helium C++ Example\",\n  \"version\": \"1.0.0\",\n  \"api_version\": 1,\n  \"library\": \"$<TARGET_FILE_NAME:helium_cpp_example>\"\n}\n"
)

add_executable(helium_header_c_smoke tests/header_c_smoke.c)
target_include_directories(helium_header_c_smoke PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include" "${DINIT_INCLUDE_DIR}")

add_executable(helium_host_contract_smoke tests/host_contract_smoke.cpp)
target_include_directories(helium_host_contract_smoke PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include" "${DINIT_INCLUDE_DIR}")
if(CMAKE_DL_LIBS)
    target_link_libraries(helium_host_contract_smoke PRIVATE ${CMAKE_DL_LIBS})
endif()

if(BUILD_TESTING)
    add_test(NAME c_header_contract COMMAND helium_header_c_smoke)
    add_test(NAME native_plugin_contract COMMAND helium_host_contract_smoke $<TARGET_FILE:helium_cpp_example>)
endif()

install(FILES include/helium/helium.h DESTINATION include/helium)
install(FILES "${DINIT_INCLUDE_DIR}/dinit/dinit.h" DESTINATION include/dinit)
install(FILES manifest/helium-plugin.schema.json DESTINATION share/helium-sdk/manifest)
