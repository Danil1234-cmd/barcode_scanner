cmake_minimum_required(VERSION 3.5)

project(barcode_desktop_app VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Multimedia MultimediaWidgets )
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

# Поиск Zint библиотеки вручную (без pkg-config)
find_library(ZINT_LIBRARY NAMES zint)
find_path(ZINT_INCLUDE_DIRS "zint.h")

if(NOT ZINT_LIBRARY)
    message(FATAL_ERROR "Zint library not found")
endif()

if(NOT ZINT_INCLUDE_DIRS)
    message(FATAL_ERROR "Zint headers not found")
endif()

# Поиск других библиотек через pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZBAR REQUIRED zbar)
pkg_check_modules(SQLITE3 REQUIRED sqlite3)

include_directories(${ZINT_INCLUDE_DIRS} ${ZBAR_INCLUDE_DIRS} ${SQLITE3_INCLUDE_DIRS})
link_directories(${ZBAR_LIBRARY_DIRS} ${SQLITE3_LIBRARY_DIRS})

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        GenerateWindow.cpp
        GenerateWindow.h
        GenerateWindow.ui
        ScannerWindow.h
        ScannerWindow.ui
        ScannerWindow.cpp
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(barcode_desktop_app
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    if(ANDROID)
        add_library(barcode_desktop_app SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(barcode_desktop_app
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(barcode_desktop_app PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
target_link_libraries(barcode_desktop_app PRIVATE ${ZINT_LIBRARY} ${ZBAR_LIBRARIES} ${SQLITE3_LIBRARIES})

if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.barcode_desktop_app)
endif()
set_target_properties(barcode_desktop_app PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS barcode_desktop_app
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(barcode_desktop_app)
endif()
