cmake_minimum_required(VERSION 3.24)
project(alienware_backend VERSION 0.4.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(FetchContent)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(PkgConfig REQUIRED)
pkg_check_modules(USB REQUIRED IMPORTED_TARGET libusb-1.0)
pkg_check_modules(HID REQUIRED IMPORTED_TARGET hidapi-hidraw)
find_package(nlohmann_json 3.11 REQUIRED)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../upstream.lock.json" lock)
foreach(name awcc alienfx loguru)
  string(JSON url GET "${lock}" ${name} repository)
  string(JSON revision GET "${lock}" ${name} revision)
  FetchContent_Declare(${name} GIT_REPOSITORY "${url}" GIT_TAG "${revision}"
    GIT_CONFIG core.autocrlf=false core.eol=lf SOURCE_SUBDIR __source_only)
  FetchContent_MakeAvailable(${name})
  set(${name}_revision "${revision}")
endforeach()
file(SHA256 "${awcc_SOURCE_DIR}/database.json" actual_database)
file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/../database.json" expected_database)
if(NOT actual_database STREQUAL expected_database)
  message(FATAL_ERROR "database.json does not match the pinned AWCC revision; run scripts/sync-backend.py")
endif()
set(generated "${CMAKE_CURRENT_BINARY_DIR}/awcc")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/prepare-awcc.py"
  "${awcc_SOURCE_DIR}" "${generated}" "${alienfx_SOURCE_DIR}" COMMAND_ERROR_IS_FATAL ANY)
add_executable(alienware-backend main.cpp direct-acpi.cpp
  "${generated}/AcpiUtils.cpp" "${generated}/Thermals.cpp"
  "${awcc_SOURCE_DIR}/src/helper.cpp"
  "${alienfx_SOURCE_DIR}/AlienFan-SDK/src/AlienFan-SDK.cpp"
  "${generated}/AlienFX_SDK.cpp"
  "${generated}/libusb_helper.cpp"
  "${loguru_SOURCE_DIR}/loguru.cpp")
target_include_directories(alienware-backend PRIVATE "${generated}" "${CMAKE_CURRENT_SOURCE_DIR}"
  "${awcc_SOURCE_DIR}/include" "${alienfx_SOURCE_DIR}/AlienFan-SDK/include"
  "${alienfx_SOURCE_DIR}/AlienFX-SDK/include" "${loguru_SOURCE_DIR}")
target_compile_definitions(alienware-backend PRIVATE NDEBUG LOGURU_WITH_STREAMS=1 VERSION="1.19.0"
  AWCC_REVISION="${awcc_revision}" ALIENFX_REVISION="${alienfx_revision}")
target_link_libraries(alienware-backend PRIVATE PkgConfig::USB PkgConfig::HID nlohmann_json::nlohmann_json dl pthread)
install(TARGETS alienware-backend RUNTIME DESTINATION lib/omarchy-alienware)
install(FILES "${awcc_SOURCE_DIR}/database.json" DESTINATION share/omarchy-alienware/awcc)
install(FILES "${awcc_SOURCE_DIR}/LICENSE" DESTINATION share/licenses/omarchy-alienware RENAME AWCC-GPL-3.0)
install(FILES "${alienfx_SOURCE_DIR}/LICENCE" DESTINATION share/licenses/omarchy-alienware RENAME AlienFX-GPL-3.0)
install(FILES "${loguru_SOURCE_DIR}/LICENSE" DESTINATION share/licenses/omarchy-alienware RENAME loguru-LICENSE)
include(CTest)
if(BUILD_TESTING)
  add_executable(backend-contracts-test contracts-test.cpp)
  target_link_libraries(backend-contracts-test PRIVATE nlohmann_json::nlohmann_json)
  add_test(NAME backend-contracts COMMAND backend-contracts-test)
  add_test(NAME backend-version COMMAND alienware-backend --version)
endif()
