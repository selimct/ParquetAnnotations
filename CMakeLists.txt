cmake_minimum_required(VERSION 3.16)
project(ParquetAnnotations)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Core Gui Widgets PrintSupport REQUIRED)
find_package(Arrow REQUIRED)
find_package(Parquet REQUIRED)

# Find QCustomPlot
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(QCUSTOMPLOT qcustomplot-qt6)
endif()

if(NOT QCUSTOMPLOT_FOUND)
    # Fallback: try to find header and link directly
    find_path(QCUSTOMPLOT_INCLUDE_DIR qcustomplot.h PATHS /usr/include)
    find_library(QCUSTOMPLOT_LIB qcustomplot-qt6 PATHS /usr/lib64 /usr/lib)
    if(NOT QCUSTOMPLOT_INCLUDE_DIR OR NOT QCUSTOMPLOT_LIB)
        message(FATAL_ERROR "QCustomPlot not found. Install qcustomplot-qt6 package.")
    endif()
endif()

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/parquet_reader.cpp
    src/parquet_reader.h
)

target_link_libraries(${PROJECT_NAME} 
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::PrintSupport
    Arrow::arrow_shared
    Parquet::parquet_shared
)

# Link QCustomPlot
if(QCUSTOMPLOT_FOUND)
    target_link_libraries(${PROJECT_NAME} ${QCUSTOMPLOT_LIBRARIES})
    target_include_directories(${PROJECT_NAME} PRIVATE ${QCUSTOMPLOT_INCLUDE_DIRS})
else()
    target_link_libraries(${PROJECT_NAME} ${QCUSTOMPLOT_LIB})
    target_include_directories(${PROJECT_NAME} PRIVATE ${QCUSTOMPLOT_INCLUDE_DIR})
endif()

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

