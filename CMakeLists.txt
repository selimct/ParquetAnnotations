cmake_minimum_required(VERSION 3.16)
project(ParquetAnnotations)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Core Gui Widgets PrintSupport REQUIRED)
find_package(Arrow REQUIRED)
find_package(Parquet REQUIRED)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/parquet_reader.cpp
    src/parquet_reader.h
)

target_link_libraries(${PROJECT_NAME}
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::PrintSupport
    Arrow::arrow_shared Parquet::parquet_shared
    qcustomplot
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
