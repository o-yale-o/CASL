# casl-core: pure C++17 CASL/COMET II assembler + virtual machine library.
add_library(casl-core STATIC
    src/Assembler.cpp
    src/Machine.cpp
    src/OpCode.cpp
)

add_library(casl::core ALIAS casl-core)

target_include_directories(casl-core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(casl-core PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(casl-core PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(casl-core PRIVATE -Wall -Wextra)
endif()
