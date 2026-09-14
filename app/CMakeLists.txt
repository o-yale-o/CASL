# Qt Widgets desktop application for the CASL/COMET environment.
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

# QScintilla: use find_package if available, else the prebuilt copy under
# third_party/qscintilla (built against the same Qt), else QPlainTextEdit.
find_package(QScintilla QUIET)
set(szEditorFallback "QPlainTextEdit (built-in fallback)")
if(TARGET QScintilla::QScintilla)
    set(CASL_QSCINTILLA_TARGET QScintilla::QScintilla)
    set(CASL_HAVE_QSCINTILLA TRUE)
    set(szEditorFallback "QScintilla (system)")
elseif(QSCINTILLA_FOUND)
    set(CASL_QSCINTILLA_TARGET ${QSCINTILLA_LIBRARIES})
    set(CASL_HAVE_QSCINTILLA TRUE)
    set(szEditorFallback "QScintilla (system)")
else()
    set(strQsciRoot "${CMAKE_SOURCE_DIR}/third_party/qscintilla")
    if(EXISTS "${strQsciRoot}/include/Qsci/qsciscintilla.h"
       AND EXISTS "${strQsciRoot}/lib/qscintilla2_qt6.lib")
        set(CASL_QSCINTILLA_TARGET qscintilla2_qt6)
        set(CASL_HAVE_QSCINTILLA TRUE)
        set(szEditorFallback "QScintilla (third_party)")
        add_library(qscintilla2_qt6 SHARED IMPORTED GLOBAL)
        set_target_properties(qscintilla2_qt6 PROPERTIES
            IMPORTED_IMPLIB "${strQsciRoot}/lib/qscintilla2_qt6.lib"
            IMPORTED_IMPLIB_RELEASE "${strQsciRoot}/lib/qscintilla2_qt6.lib"
            IMPORTED_IMPLIB_DEBUG "${strQsciRoot}/lib/qscintilla2_qt6.lib"
            IMPORTED_LOCATION "${strQsciRoot}/bin/qscintilla2_qt6.dll"
            IMPORTED_LOCATION_RELEASE "${strQsciRoot}/bin/qscintilla2_qt6.dll"
            IMPORTED_LOCATION_DEBUG "${strQsciRoot}/bin/qscintilla2_qt6.dll"
            INTERFACE_INCLUDE_DIRECTORIES "${strQsciRoot}/include"
        )
        set(QSCINTILLA_DLL "${strQsciRoot}/bin/qscintilla2_qt6.dll")
    endif()
endif()
message(STATUS "CASLStudio editor backend: ${szEditorFallback}")

qt_add_executable(CASLStudio
    main.cpp
    CodeEditor.hpp
    CodeEditor.cpp
    CommandHelp.hpp
    CommandHelp.cpp
    MachineRunner.hpp
    MachineRunner.cpp
    MainWindow.hpp
    MainWindow.cpp
    Panels.hpp
    Panels.cpp
)

target_link_libraries(CASLStudio PRIVATE
    Qt6::Widgets
    casl::core
)

if(CASL_HAVE_QSCINTILLA)
    target_compile_definitions(CASLStudio PRIVATE CASL_HAVE_QSCINTILLA QSCINTILLA_DLL)
    target_link_libraries(CASLStudio PRIVATE ${CASL_QSCINTILLA_TARGET})
    if(QSCINTILLA_DLL)
        add_custom_command(TARGET CASLStudio POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${QSCINTILLA_DLL}" "$<TARGET_FILE_DIR:CASLStudio>"
            COMMENT "Copying qscintilla2_qt6.dll next to CASLStudio"
        )
    endif()
endif()

set_target_properties(CASLStudio PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)

if(MSVC)
    target_compile_options(CASLStudio PRIVATE /W3 /permissive- /EHsc)
else()
    target_compile_options(CASLStudio PRIVATE -Wall -Wextra)
endif()

# Windows deployment helper: run windeployqt after building. The QScintilla
# DLL is passed too so its dependencies (e.g. Qt6PrintSupport.dll) are
# deployed next to the executable as well.
if(WIN32 AND TARGET Qt6::windeployqt)
    if(QSCINTILLA_DLL)
        add_custom_command(TARGET CASLStudio POST_BUILD
            COMMAND Qt6::windeployqt "$<TARGET_FILE:CASLStudio>" "${QSCINTILLA_DLL}"
            COMMENT "Running windeployqt on CASLStudio + QScintilla"
        )
    else()
        add_custom_command(TARGET CASLStudio POST_BUILD
            COMMAND Qt6::windeployqt "$<TARGET_FILE:CASLStudio>"
            COMMENT "Running windeployqt on CASLStudio"
        )
    endif()
endif()

# Extra safety net: always copy the Qt6PrintSupport DLL (QScintilla printer
# support) next to the executable if it can be located in the Qt install.
if(WIN32 AND EXISTS "${CMAKE_PREFIX_PATH}")
    file(GLOB arrPrintDlls "${CMAKE_PREFIX_PATH}/bin/Qt6PrintSupport.dll")
    foreach(strDll ${arrPrintDlls})
        add_custom_command(TARGET CASLStudio POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${strDll}" "$<TARGET_FILE_DIR:CASLStudio>"
            COMMENT "Copying Qt6PrintSupport.dll"
        )
    endforeach()
endif()

# Ship the sample programs next to the executable.
add_custom_command(TARGET CASLStudio POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/samples" "$<TARGET_FILE_DIR:CASLStudio>/samples"
    COMMENT "Copying sample programs"
)

# Headless regression test for the debug runner (breakpoint pause + F10).
qt_add_executable(test_runner tests/test_runner.cpp MachineRunner.cpp)
target_include_directories(test_runner PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_runner PRIVATE Qt6::Core casl::core)
if(MSVC)
    target_compile_options(test_runner PRIVATE /W3 /permissive- /EHsc)
else()
    target_compile_options(test_runner PRIVATE -Wall -Wextra)
endif()
add_test(NAME runner COMMAND test_runner)
