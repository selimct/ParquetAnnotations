# ParquetAnnotations

Interactive desktop application for visualizing and annotating ECG data from parquet files.

## Features

- **Single unified plot with:**
  - Blue line: ECG1 (rotated) waveform
  - Orange line: Pulse signal
  - Semi-transparent red dashed lines: Target annotations (1 only)
- **Interactive controls:**
  - Zoom: scroll wheel
  - Pan: drag mouse
  - Hover: see exact coordinates
- **Efficient**: handles up to 350k entries smoothly
- **One file at a time**: load and visualize parquet files sequentially

## Installation

### Dependencies

- Qt 6
- Apache Arrow C++
- Parquet C++
- QCustomPlot

### Ubuntu/Debian

```bash
sudo apt-get install qt6-base-dev libapache-arrow-dev libparquet-dev
```

### Fedora/RHEL

```bash
sudo dnf install qt6-qtbase-devel libarrow-devel parquet-libs-devel
```

### QCustomPlot Setup (if needed)

If QCustomPlot is not in your system packages:

1. Download from <https://www.qcustomplot.com/>
2. Copy `qcustomplot.h` and `qcustomplot.cpp` to `src/`
3. Uncomment lines in CMakeLists.txt to include source files

### Build

```bash
mkdir build
cd build
cmake ..
make
```

## Usage

```bash
./ParquetAnnotations
```

1. Click **"Open Parquet File"** button
2. Select a `.parquet` file
3. View the three overlaid signals:
   - ECG1 waveform (blue)
   - Pulse signal (orange)
   - Target annotations (red dashed lines at flagged positions)
4. Use mouse wheel to zoom, drag to pan
5. Hover over any point to read coordinates

## Data Format

Your parquet file must contain these columns:

- `raw_idx` (numeric): sample index
- `ecg1_rotated` (numeric): ECG signal
- `puls_raw` (numeric): pulse/heart rate signal
- `target` (integer): annotation flags (0 or 1)

Only target=1 markers are displayed as vertical lines, positioned slightly above the ECG value at each annotation point.
