#include "mainwindow.h"
#include "parquet_reader.h"
#include "qcustomplot.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QLabel>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QToolBar>
#include <QDoubleSpinBox>
#include <algorithm>
#include <cmath>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent) {
    setWindowTitle("Parquet ECG Viewer");
    setGeometry(100, 100, 1400, 900);

    for (int i = 0; i < 4; ++i) {
        selection_rect[i] = nullptr;
    }

    setupUI();
    setupToolbar();
}

MainWindow::~MainWindow() {
}

void MainWindow::setupUI() {
    QWidget* central = new QWidget(this);
    setCentralWidget(central);

    QVBoxLayout* main_layout = new QVBoxLayout(central);

    QHBoxLayout* button_layout = new QHBoxLayout();
    QPushButton* open_btn = new QPushButton("Open Parquet File");
    connect(open_btn, &QPushButton::clicked, this, &MainWindow::openFile);
    button_layout->addWidget(open_btn);
    button_layout->addStretch();
    main_layout->addLayout(button_layout);

    plot = new QCustomPlot();
    plot->setInteraction(QCP::iRangeDrag, true);
    plot->setInteraction(QCP::iRangeZoom, true);
    plot->xAxis->setLabel("raw_idx");
    plot->yAxis->setLabel("Values");
    plot->installEventFilter(this);
    main_layout->addWidget(plot);

    central->setLayout(main_layout);
}

void MainWindow::setupToolbar() {
    QToolBar* toolbar = addToolBar("Zoom");
    toolbar->setIconSize(QSize(16, 16));

    box_zoom_btn = new QPushButton("Box Zoom");
    connect(box_zoom_btn, &QPushButton::clicked, this, &MainWindow::setBoxZoomMode);
    box_zoom_btn->setCheckable(true);
    toolbar->addWidget(box_zoom_btn);

    constrained_zoom_btn = new QPushButton("Constrained Zoom");
    connect(constrained_zoom_btn, &QPushButton::clicked, this, &MainWindow::setConstrainedZoomMode);
    constrained_zoom_btn->setCheckable(true);
    toolbar->addWidget(constrained_zoom_btn);

    add_marker_btn = new QPushButton("Add Marker");
    connect(add_marker_btn, &QPushButton::clicked, this, &MainWindow::setAddMarkerMode);
    add_marker_btn->setCheckable(true);
    toolbar->addWidget(add_marker_btn);

    toolbar->addSeparator();

    toolbar->addAction("Zoom In", this, &MainWindow::zoomIn);
    toolbar->addAction("Zoom Out", this, &MainWindow::zoomOut);
    toolbar->addAction("Fit", this, &MainWindow::fitView);

    undo_btn = new QPushButton("Undo (Ctrl+Z)");
    connect(undo_btn, &QPushButton::clicked, this, &MainWindow::undoZoom);
    undo_btn->setEnabled(false);
    toolbar->addWidget(undo_btn);

    toolbar->addSeparator();

    toolbar->addWidget(new QLabel("Target Height:"));
    target_height_spin = new QDoubleSpinBox();
    target_height_spin->setRange(-1e6, 1e6);
    target_height_spin->setSingleStep(0.1);
    target_height_spin->setDecimals(4);
    target_height_spin->setValue(0.0);
    toolbar->addWidget(target_height_spin);

    QPushButton* apply_btn = new QPushButton("Apply");
    connect(apply_btn, &QPushButton::clicked, this, &MainWindow::applyTargetHeight);
    toolbar->addWidget(apply_btn);
}

void MainWindow::updateModeButtons() {
    box_zoom_btn->setChecked(current_mode == InteractionMode::BoxZoom);
    constrained_zoom_btn->setChecked(current_mode == InteractionMode::ConstrainedZoom);
    add_marker_btn->setChecked(current_mode == InteractionMode::AddMarker);
}

void MainWindow::setBoxZoomMode() {
    current_mode = (current_mode == InteractionMode::BoxZoom) ? InteractionMode::Normal : InteractionMode::BoxZoom;
    updateModeButtons();
}

void MainWindow::setConstrainedZoomMode() {
    current_mode = (current_mode == InteractionMode::ConstrainedZoom) ? InteractionMode::Normal : InteractionMode::ConstrainedZoom;
    updateModeButtons();
}

void MainWindow::setAddMarkerMode() {
    current_mode = (current_mode == InteractionMode::AddMarker) ? InteractionMode::Normal : InteractionMode::AddMarker;
    updateModeButtons();
}

void MainWindow::zoomIn() {
    saveZoomState();
    double x_range = plot->xAxis->range().size();
    double y_range = plot->yAxis->range().size();
    double x_center = plot->xAxis->range().center();
    double y_center = plot->yAxis->range().center();

    plot->xAxis->setRange(x_center - x_range * 0.4, x_center + x_range * 0.4);
    plot->yAxis->setRange(y_center - y_range * 0.4, y_center + y_range * 0.4);
    plot->replot();
}

void MainWindow::zoomOut() {
    saveZoomState();
    double x_range = plot->xAxis->range().size();
    double y_range = plot->yAxis->range().size();
    double x_center = plot->xAxis->range().center();
    double y_center = plot->yAxis->range().center();

    plot->xAxis->setRange(x_center - x_range * 0.6, x_center + x_range * 0.6);
    plot->yAxis->setRange(y_center - y_range * 0.6, y_center + y_range * 0.6);
    plot->replot();
}

void MainWindow::fitView() {
    plot->rescaleAxes();
    plot->replot();
}

void MainWindow::saveZoomState() {
    ZoomState state;
    state.x_min = plot->xAxis->range().lower;
    state.x_max = plot->xAxis->range().upper;
    state.y_min = plot->yAxis->range().lower;
    state.y_max = plot->yAxis->range().upper;
    zoom_history.push(state);
    undo_btn->setEnabled(true);
}

void MainWindow::undoZoom() {
    if (zoom_history.isEmpty()) return;
    ZoomState state = zoom_history.pop();
    plot->xAxis->setRange(state.x_min, state.x_max);
    plot->yAxis->setRange(state.y_min, state.y_max);
    plot->replot();
    if (zoom_history.isEmpty()) {
        undo_btn->setEnabled(false);
    }
}

void MainWindow::applyTargetHeight() {
    target_height = target_height_spin->value();
    if (current_data) {
        setupPlot(current_data);
    }
}

void MainWindow::openFile() {
    QString filename = QFileDialog::getOpenFileName(this, "Open Parquet File", "", "Parquet Files (*.parquet)");
    if (filename.isEmpty()) return;

    try {
        current_data = ParquetReader::read(filename.toStdString());
        setupPlot(current_data);
    } catch (const std::exception& e) {
        qWarning() << "Error reading file:" << e.what();
    }
}

void MainWindow::setupPlot(const std::shared_ptr<ParquetData>& data) {
    markers.clear();
    next_marker_id = 0;

    plot->clearPlottables();
    plot->clearItems();

    // Calculate 95th percentile once at file load
    std::vector<double> sorted_ecg = data->ecg1_rotated;
    std::sort(sorted_ecg.begin(), sorted_ecg.end());
    size_t percentile_idx = static_cast<size_t>(sorted_ecg.size() * 0.95);
    percentile_95 = sorted_ecg[percentile_idx];
    target_height_spin->setValue(percentile_95 * 0.95);

    QVector<double> x(data->raw_idx.begin(), data->raw_idx.end());

    QVector<double> y_ecg(data->ecg1_rotated.begin(), data->ecg1_rotated.end());
    plot->addGraph();
    plot->graph(0)->setData(x, y_ecg);
    plot->graph(0)->setName("ECG1");
    plot->graph(0)->setLineStyle(QCPGraph::lsLine);
    plot->graph(0)->setPen(QPen(Qt::blue, 1));

    QVector<double> y_pulse(data->puls_raw.begin(), data->puls_raw.end());
    plot->addGraph();
    plot->graph(1)->setData(x, y_pulse);
    plot->graph(1)->setName("Pulse");
    plot->graph(1)->setLineStyle(QCPGraph::lsLine);
    plot->graph(1)->setPen(QPen(Qt::red, 1));

    plot->rescaleAxes();
    plot->legend->setVisible(true);
    plot->replot();
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event) {
    if (obj == plot) {
        if (event->type() == QEvent::MouseButtonPress) {
            handleMousePress(static_cast<QMouseEvent*>(event));
            return true;
        } else if (event->type() == QEvent::MouseMove) {
            handleMouseMove(static_cast<QMouseEvent*>(event));
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease) {
            handleMouseRelease(static_cast<QMouseEvent*>(event));
            return true;
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::keyPressEvent(QKeyEvent* event) {
    if (!plot) return;

    double pan_factor = 0.1;  // Pan by 10% of current range

    switch (event->key()) {
        case Qt::Key_Left:
            {
                double range = plot->xAxis->range().size();
                plot->xAxis->setRange(plot->xAxis->range().lower - range * pan_factor,
                                     plot->xAxis->range().upper - range * pan_factor);
                plot->replot();
            }
            return;
        case Qt::Key_Right:
            {
                double range = plot->xAxis->range().size();
                plot->xAxis->setRange(plot->xAxis->range().lower + range * pan_factor,
                                     plot->xAxis->range().upper + range * pan_factor);
                plot->replot();
            }
            return;
        case Qt::Key_Up:
            {
                double range = plot->yAxis->range().size();
                plot->yAxis->setRange(plot->yAxis->range().lower + range * pan_factor,
                                     plot->yAxis->range().upper + range * pan_factor);
                plot->replot();
            }
            return;
        case Qt::Key_Down:
            {
                double range = plot->yAxis->range().size();
                plot->yAxis->setRange(plot->yAxis->range().lower - range * pan_factor,
                                     plot->yAxis->range().upper - range * pan_factor);
                plot->replot();
            }
            return;
        default:
            break;
    }

    QMainWindow::keyPressEvent(event);
}

void MainWindow::handleMousePress(QMouseEvent* event) {
    if (current_mode == InteractionMode::BoxZoom) {
        drag_start = event->pos();
        dragging = true;
    } else if (current_mode == InteractionMode::ConstrainedZoom) {
        drag_start = event->pos();
        last_mouse_pos = event->pos();
        dragging = true;
        constrain_axis = -1;
        saveZoomState();
    } else if (current_mode == InteractionMode::AddMarker) {
        addMarkerAtCurve(event->pos());
    }
}

void MainWindow::handleMouseMove(QMouseEvent* event) {
    if (!dragging) return;

    if (current_mode == InteractionMode::BoxZoom) {
        if (selection_rect[0]) {
            plot->removeItem(selection_rect[0]);
            plot->removeItem(selection_rect[1]);
            plot->removeItem(selection_rect[2]);
            plot->removeItem(selection_rect[3]);
        }

        int x1 = drag_start.x(), y1 = drag_start.y();
        int x2 = event->pos().x(), y2 = event->pos().y();

        selection_rect[0] = new QCPItemLine(plot);
        selection_rect[0]->start->setCoords(plot->xAxis->pixelToCoord(x1), plot->yAxis->pixelToCoord(y1));
        selection_rect[0]->end->setCoords(plot->xAxis->pixelToCoord(x2), plot->yAxis->pixelToCoord(y1));
        selection_rect[0]->setPen(QPen(Qt::gray, 1, Qt::DashLine));

        selection_rect[1] = new QCPItemLine(plot);
        selection_rect[1]->start->setCoords(plot->xAxis->pixelToCoord(x2), plot->yAxis->pixelToCoord(y1));
        selection_rect[1]->end->setCoords(plot->xAxis->pixelToCoord(x2), plot->yAxis->pixelToCoord(y2));
        selection_rect[1]->setPen(QPen(Qt::gray, 1, Qt::DashLine));

        selection_rect[2] = new QCPItemLine(plot);
        selection_rect[2]->start->setCoords(plot->xAxis->pixelToCoord(x2), plot->yAxis->pixelToCoord(y2));
        selection_rect[2]->end->setCoords(plot->xAxis->pixelToCoord(x1), plot->yAxis->pixelToCoord(y2));
        selection_rect[2]->setPen(QPen(Qt::gray, 1, Qt::DashLine));

        selection_rect[3] = new QCPItemLine(plot);
        selection_rect[3]->start->setCoords(plot->xAxis->pixelToCoord(x1), plot->yAxis->pixelToCoord(y2));
        selection_rect[3]->end->setCoords(plot->xAxis->pixelToCoord(x1), plot->yAxis->pixelToCoord(y1));
        selection_rect[3]->setPen(QPen(Qt::gray, 1, Qt::DashLine));

        plot->replot();
    } else if (current_mode == InteractionMode::ConstrainedZoom) {
        QPoint delta = event->pos() - drag_start;
        int dx = std::abs(delta.x());
        int dy = std::abs(delta.y());

        // Determine axis on first significant movement
        if (constrain_axis == -1 && (dx > 50 || dy > 50)) {
            constrain_axis = (dx > dy) ? 0 : 1;  // 0 = horizontal (x), 1 = vertical (y)
        }

        // Only zoom if we've determined the axis
        if (constrain_axis == 0) {
            // Horizontal zoom (x-axis)
            double x_range = plot->xAxis->range().size();
            double x_center = plot->xAxis->range().center();
            double zoom_factor = 1.0 + (delta.x() * 0.003);
            double new_half_range = x_range / (2.0 * zoom_factor);
            plot->xAxis->setRange(x_center - new_half_range, x_center + new_half_range);
            plot->replot();
        } else if (constrain_axis == 1) {
            // Vertical zoom (y-axis)
            double y_range = plot->yAxis->range().size();
            double y_center = plot->yAxis->range().center();
            double zoom_factor = 1.0 - (delta.y() * 0.003);
            double new_half_range = y_range / (2.0 * zoom_factor);
            plot->yAxis->setRange(y_center - new_half_range, y_center + new_half_range);
            plot->replot();
        }
    }
}

void MainWindow::handleMouseRelease(QMouseEvent* event) {
    if (!dragging) return;
    dragging = false;

    if (current_mode == InteractionMode::BoxZoom) {
        if (selection_rect[0]) {
            plot->removeItem(selection_rect[0]);
            plot->removeItem(selection_rect[1]);
            plot->removeItem(selection_rect[2]);
            plot->removeItem(selection_rect[3]);
            for (int i = 0; i < 4; ++i) selection_rect[i] = nullptr;
        }

        double x1 = plot->xAxis->pixelToCoord(drag_start.x());
        double y1 = plot->yAxis->pixelToCoord(drag_start.y());
        double x2 = plot->xAxis->pixelToCoord(event->pos().x());
        double y2 = plot->yAxis->pixelToCoord(event->pos().y());

        if (std::abs(x2 - x1) > 1 && std::abs(y2 - y1) > 1) {
            saveZoomState();
            plot->xAxis->setRange(std::min(x1, x2), std::max(x1, x2));
            plot->yAxis->setRange(std::min(y1, y2), std::max(y1, y2));
            plot->replot();
        }
    }
}

void MainWindow::addMarkerAtCurve(QPoint pixel_pos) {
    if (!current_data) return;

    int best_curve = -1;
    int best_idx = -1;
    double best_dist = 10.0;

    for (int curve = 0; curve < plot->graphCount(); ++curve) {
        int idx;
        double dist = findNearestPointOnCurve(curve, pixel_pos, idx);
        if (dist < best_dist) {
            best_dist = dist;
            best_curve = curve;
            best_idx = idx;
        }
    }

    if (best_curve == -1) return;

    QCPGraph* graph = plot->graph(best_curve);
    double x = graph->data()->at(best_idx)->key;
    double y = graph->data()->at(best_idx)->value;

    QColor color = (best_curve == 0) ? Qt::blue : (best_curve == 1) ? Qt::green : Qt::red;

    QCPItemText* label = new QCPItemText(plot);
    label->setColor(color);
    label->setPositionAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
    label->position->setCoords(x, y);
    label->setText(QString("x: %1\ny: %2").arg(x, 0, 'f', 1).arg(y, 0, 'f', 3));
    label->setClipToAxisRect(false);

    MarkerData marker;
    marker.x = x;
    marker.y = y;
    marker.curve_index = best_curve;
    marker.label = label;

    int marker_id = next_marker_id++;
    markers[marker_id] = marker;

    plot->replot();
}

double MainWindow::findNearestPointOnCurve(int curve_idx, QPoint pixel_pos, int& point_idx) {
    QCPGraph* graph = plot->graph(curve_idx);
    if (!graph || graph->data()->size() == 0) return 1e10;

    double min_dist = 1e10;
    int best_idx = 0;

    for (int i = 0; i < graph->data()->size(); ++i) {
        double x = graph->data()->at(i)->key;
        double y = graph->data()->at(i)->value;
        QPoint pt(plot->xAxis->coordToPixel(x), plot->yAxis->coordToPixel(y));

        double dist = std::hypot(pt.x() - pixel_pos.x(), pt.y() - pixel_pos.y());
        if (dist < min_dist) {
            min_dist = dist;
            best_idx = i;
        }
    }

    point_idx = best_idx;
    return min_dist;
}

void MainWindow::deleteMarker(int id) {
    if (markers.contains(id)) {
        plot->removeItem(markers[id].label);
        markers.remove(id);
        plot->replot();
    }
}
