#include "mainwindow.h"
#include "parquet_reader.h"
#include "qcustomplot.h"
#include <QApplication>
#include <QClipboard>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QFile>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QStringList>
#include <QToolBar>
#include <QTextStream>
#include <QDoubleSpinBox>
#include <algorithm>
#include <cmath>
#include <limits>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("Parquet ECG Viewer");
    setGeometry(100, 100, 1400, 900);

    for (int i = 0; i < 4; ++i)
    {
        selection_rect[i] = nullptr;
    }

    setupUI();
    setupToolbar();
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUI()
{
    QWidget *central = new QWidget(this);
    setCentralWidget(central);

    QVBoxLayout *main_layout = new QVBoxLayout(central);

    QHBoxLayout *button_layout = new QHBoxLayout();
    QPushButton *open_btn = new QPushButton("Open Parquet File");
    connect(open_btn, &QPushButton::clicked, this, &MainWindow::openFile);
    button_layout->addWidget(open_btn);
    button_layout->addStretch();
    main_layout->addLayout(button_layout);

    plot = new QCustomPlot();
    plot->setInteraction(QCP::iRangeDrag, true);
    plot->setInteraction(QCP::iRangeZoom, true);
    plot->xAxis->setLabel("raw_idx");
    plot->yAxis->setLabel("Values");
    connect(plot->xAxis, QOverload<const QCPRange &>::of(&QCPAxis::rangeChanged),
            this, [this](const QCPRange &) { updateMarkerLabelPositions(); });
    connect(plot->yAxis, QOverload<const QCPRange &>::of(&QCPAxis::rangeChanged),
            this, [this](const QCPRange &) { updateMarkerLabelPositions(); });
    plot->installEventFilter(this);
    main_layout->addWidget(plot);

    central->setLayout(main_layout);
}

void MainWindow::setupToolbar()
{
    QToolBar *toolbar = addToolBar("Zoom");
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

    add_fp_marker_btn = new QPushButton("Add FP");
    connect(add_fp_marker_btn, &QPushButton::clicked, this, &MainWindow::setAddFalsePositiveMode);
    add_fp_marker_btn->setCheckable(true);
    toolbar->addWidget(add_fp_marker_btn);

    add_fn_marker_btn = new QPushButton("Add FN");
    connect(add_fn_marker_btn, &QPushButton::clicked, this, &MainWindow::setAddFalseNegativeMode);
    add_fn_marker_btn->setCheckable(true);
    toolbar->addWidget(add_fn_marker_btn);

    toolbar->addAction("Export Markers", this, &MainWindow::exportMarkers);

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

    QPushButton *apply_btn = new QPushButton("Apply");
    connect(apply_btn, &QPushButton::clicked, this, &MainWindow::applyTargetHeight);
    toolbar->addWidget(apply_btn);
}

void MainWindow::updateModeButtons()
{
    box_zoom_btn->setChecked(current_mode == InteractionMode::BoxZoom);
    constrained_zoom_btn->setChecked(current_mode == InteractionMode::ConstrainedZoom);
    add_marker_btn->setChecked(current_mode == InteractionMode::AddMarker &&
                               current_marker_type == MarkerType::Generic);
    add_fp_marker_btn->setChecked(current_mode == InteractionMode::AddMarker &&
                                  current_marker_type == MarkerType::FalsePositive);
    add_fn_marker_btn->setChecked(current_mode == InteractionMode::AddMarker &&
                                  current_marker_type == MarkerType::FalseNegative);
}

void MainWindow::setBoxZoomMode()
{
    current_mode = (current_mode == InteractionMode::BoxZoom) ? InteractionMode::Normal : InteractionMode::BoxZoom;
    updateModeButtons();
}

void MainWindow::setConstrainedZoomMode()
{
    current_mode = (current_mode == InteractionMode::ConstrainedZoom) ? InteractionMode::Normal : InteractionMode::ConstrainedZoom;
    updateModeButtons();
}

void MainWindow::setAddMarkerMode()
{
    bool already_active = current_mode == InteractionMode::AddMarker &&
                          current_marker_type == MarkerType::Generic;
    current_marker_type = MarkerType::Generic;
    current_mode = already_active ? InteractionMode::Normal : InteractionMode::AddMarker;
    updateModeButtons();
}

void MainWindow::setAddFalsePositiveMode()
{
    bool already_active = current_mode == InteractionMode::AddMarker &&
                          current_marker_type == MarkerType::FalsePositive;
    current_marker_type = MarkerType::FalsePositive;
    current_mode = already_active ? InteractionMode::Normal : InteractionMode::AddMarker;
    updateModeButtons();
}

void MainWindow::setAddFalseNegativeMode()
{
    bool already_active = current_mode == InteractionMode::AddMarker &&
                          current_marker_type == MarkerType::FalseNegative;
    current_marker_type = MarkerType::FalseNegative;
    current_mode = already_active ? InteractionMode::Normal : InteractionMode::AddMarker;
    updateModeButtons();
}

void MainWindow::exportMarkers()
{
    QString filename = QFileDialog::getSaveFileName(this, "Export Markers", "", "Text Files (*.txt);;All Files (*)");
    if (filename.isEmpty())
        return;

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "Export Markers", QString("Could not write to %1").arg(filename));
        return;
    }

    QString export_text = formatMarkerLine("false_pos", MarkerType::FalsePositive) + "\n" +
                          formatMarkerLine("false_neg", MarkerType::FalseNegative) + "\n";

    QTextStream out(&file);
    out << export_text;

    QApplication::clipboard()->setText(export_text);

    QMessageBox::information(this, "Export Markers",
                             QString("Exported markers to %1 and copied them to the clipboard.").arg(filename));
}

void MainWindow::zoomIn()
{
    saveZoomState();
    double x_range = plot->xAxis->range().size();
    double y_range = plot->yAxis->range().size();
    double x_center = plot->xAxis->range().center();
    double y_center = plot->yAxis->range().center();

    plot->xAxis->setRange(x_center - x_range * 0.4, x_center + x_range * 0.4);
    plot->yAxis->setRange(y_center - y_range * 0.4, y_center + y_range * 0.4);
    updateMarkerLabelPositions();
    plot->replot();
}

void MainWindow::zoomOut()
{
    saveZoomState();
    double x_range = plot->xAxis->range().size();
    double y_range = plot->yAxis->range().size();
    double x_center = plot->xAxis->range().center();
    double y_center = plot->yAxis->range().center();

    plot->xAxis->setRange(x_center - x_range * 0.6, x_center + x_range * 0.6);
    plot->yAxis->setRange(y_center - y_range * 0.6, y_center + y_range * 0.6);
    updateMarkerLabelPositions();
    plot->replot();
}

void MainWindow::fitView()
{
    plot->rescaleAxes();
    updateMarkerLabelPositions();
    plot->replot();
}

void MainWindow::saveZoomState()
{
    ZoomState state;
    state.x_min = plot->xAxis->range().lower;
    state.x_max = plot->xAxis->range().upper;
    state.y_min = plot->yAxis->range().lower;
    state.y_max = plot->yAxis->range().upper;
    zoom_history.push(state);
    undo_btn->setEnabled(true);
}

void MainWindow::undoZoom()
{
    if (zoom_history.isEmpty())
        return;
    ZoomState state = zoom_history.pop();
    plot->xAxis->setRange(state.x_min, state.x_max);
    plot->yAxis->setRange(state.y_min, state.y_max);
    updateMarkerLabelPositions();
    plot->replot();
    if (zoom_history.isEmpty())
    {
        undo_btn->setEnabled(false);
    }
}

void MainWindow::applyTargetHeight()
{
    target_height = target_height_spin->value();
    if (current_data)
    {
        updateTargetPlot();
        plot->replot();
    }
}

void MainWindow::openFile()
{
    QString filename = QFileDialog::getOpenFileName(this, "Open Parquet File", "", "Parquet Files (*.parquet)");
    if (filename.isEmpty())
        return;

    try
    {
        current_data = ParquetReader::read(filename.toStdString());
        setupPlot(current_data);
    }
    catch (const std::exception &e)
    {
        qWarning() << "Error reading file:" << e.what();
    }
}

void MainWindow::setupPlot(const std::shared_ptr<ParquetData> &data)
{
    markers.clear();
    next_marker_id = 0;
    selected_marker_id = -1;
    target_graph = nullptr;

    plot->clearPlottables();
    plot->clearItems();

    // Calculate 95th percentile once at file load
    std::vector<double> sorted_ecg = data->ecg1_rotated;
    std::sort(sorted_ecg.begin(), sorted_ecg.end());
    size_t percentile_idx = static_cast<size_t>(sorted_ecg.size() * 0.95);
    percentile_95 = sorted_ecg[percentile_idx];
    target_height = percentile_95 * 0.95;
    target_height_spin->setValue(target_height);

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
    plot->graph(1)->setPen(QPen(QColor(255, 140, 0), 1));

    updateTargetPlot();

    plot->rescaleAxes();
    plot->legend->setVisible(true);
    plot->replot();
}

void MainWindow::updateTargetPlot()
{
    if (target_graph)
    {
        plot->removePlottable(target_graph);
        target_graph = nullptr;
    }

    if (!current_data)
        return;

    QVector<double> target_x;
    QVector<double> target_y;
    int sample_count = std::min(current_data->raw_idx.size(), current_data->target.size());
    target_x.reserve(sample_count);
    target_y.reserve(sample_count);

    for (int i = 0; i < sample_count; ++i)
    {
        if (current_data->target[i] == 1)
        {
            target_x.append(current_data->raw_idx[i]);
            target_y.append(target_height);
        }
    }

    if (target_x.isEmpty())
        return;

    plot->addGraph();
    target_graph = plot->graph(plot->graphCount() - 1);
    target_graph->setName("Target");
    target_graph->setData(target_x, target_y);
    target_graph->setLineStyle(QCPGraph::lsImpulse);
    target_graph->setScatterStyle(QCPScatterStyle::ssNone);
    target_graph->setPen(QPen(QColor(220, 0, 0, 180), 1, Qt::DashLine));

    refreshFalsePositiveMarkerPositions();
}

QString MainWindow::markerTypeName(MarkerType type) const
{
    switch (type)
    {
    case MarkerType::FalsePositive:
        return "FP";
    case MarkerType::FalseNegative:
        return "FN";
    case MarkerType::Generic:
    default:
        return "Marker";
    }
}

QColor MainWindow::markerTypeColor(MarkerType type) const
{
    switch (type)
    {
    case MarkerType::FalsePositive:
        return QColor(220, 40, 40);
    case MarkerType::FalseNegative:
        return QColor(0, 150, 120);
    case MarkerType::Generic:
    default:
        return QColor(110, 95, 180);
    }
}

QString MainWindow::markerLabelText(MarkerType type, double x, double y) const
{
    return QString("%1\nx: %2\ny: %3")
        .arg(markerTypeName(type))
        .arg(x, 0, 'f', 1)
        .arg(y, 0, 'f', 3);
}

QString MainWindow::formatMarkerLine(const QString &name, MarkerType type) const
{
    QVector<qint64> positions;
    for (auto it = markers.constBegin(); it != markers.constEnd(); ++it)
    {
        if (it.value().type == type)
            positions.append(static_cast<qint64>(std::llround(it.value().x)));
    }

    std::sort(positions.begin(), positions.end());

    QStringList values;
    values.reserve(positions.size());
    for (qint64 position : positions)
    {
        values.append(QString::number(position));
    }

    return QString("      \"%1\": [%2],").arg(name, values.join(","));
}

bool MainWindow::isGraphAllowedForMarker(MarkerType type, int curve_idx) const
{
    QCPGraph *graph = plot->graph(curve_idx);
    if (!graph)
        return false;

    bool is_target_graph = graph == target_graph;
    switch (type)
    {
    case MarkerType::FalsePositive:
        return is_target_graph;
    case MarkerType::FalseNegative:
        return curve_idx == 0 && !is_target_graph;
    case MarkerType::Generic:
    default:
        return !is_target_graph;
    }
}

void MainWindow::refreshFalsePositiveMarkerPositions()
{
    if (!target_graph)
        return;

    int target_curve = -1;
    for (int curve = 0; curve < plot->graphCount(); ++curve)
    {
        if (plot->graph(curve) == target_graph)
        {
            target_curve = curve;
            break;
        }
    }

    if (target_curve == -1)
        return;

    QVector<int> marker_ids;
    for (auto it = markers.constBegin(); it != markers.constEnd(); ++it)
    {
        if (it.value().type == MarkerType::FalsePositive)
            marker_ids.append(it.key());
    }

    for (int marker_id : marker_ids)
    {
        int point_idx = findNearestPointByKey(target_curve, markers[marker_id].x);
        if (point_idx != -1)
            updateMarkerPosition(marker_id, target_curve, point_idx);
    }
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == plot)
    {
        if (event->type() == QEvent::MouseButtonPress)
        {
            handleMousePress(static_cast<QMouseEvent *>(event));
            return true;
        }
        else if (event->type() == QEvent::MouseMove)
        {
            handleMouseMove(static_cast<QMouseEvent *>(event));
            return true;
        }
        else if (event->type() == QEvent::MouseButtonRelease)
        {
            handleMouseRelease(static_cast<QMouseEvent *>(event));
            return true;
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (!plot)
        return;

    if (selected_marker_id != -1 &&
        (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right ||
         event->key() == Qt::Key_Up || event->key() == Qt::Key_Down))
    {
        moveSelectedMarker(event->key());
        return;
    }

    double pan_factor = 0.1; // Pan by 10% of current range

    switch (event->key())
    {
    case Qt::Key_Left:
    {
        double range = plot->xAxis->range().size();
        plot->xAxis->setRange(plot->xAxis->range().lower - range * pan_factor,
                              plot->xAxis->range().upper - range * pan_factor);
        updateMarkerLabelPositions();
        plot->replot();
    }
        return;
    case Qt::Key_Right:
    {
        double range = plot->xAxis->range().size();
        plot->xAxis->setRange(plot->xAxis->range().lower + range * pan_factor,
                              plot->xAxis->range().upper + range * pan_factor);
        updateMarkerLabelPositions();
        plot->replot();
    }
        return;
    case Qt::Key_Up:
    {
        double range = plot->yAxis->range().size();
        plot->yAxis->setRange(plot->yAxis->range().lower + range * pan_factor,
                              plot->yAxis->range().upper + range * pan_factor);
        updateMarkerLabelPositions();
        plot->replot();
    }
        return;
    case Qt::Key_Down:
    {
        double range = plot->yAxis->range().size();
        plot->yAxis->setRange(plot->yAxis->range().lower - range * pan_factor,
                              plot->yAxis->range().upper - range * pan_factor);
        updateMarkerLabelPositions();
        plot->replot();
    }
        return;
    default:
        break;
    }

    QMainWindow::keyPressEvent(event);
}

void MainWindow::handleMousePress(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton)
    {
        int marker_id = findMarkerAt(event->pos());
        if (marker_id != -1)
            deleteMarker(marker_id);
        else
            selectMarker(-1);

        dragging = false;
        clearSelectionRect();
        return;
    }

    if (current_mode == InteractionMode::BoxZoom)
    {
        drag_start = event->pos();
        dragging = true;
    }
    else if (current_mode == InteractionMode::ConstrainedZoom)
    {
        drag_start = event->pos();
        dragging = true;
    }
    else if (current_mode == InteractionMode::AddMarker)
    {
        int marker_id = findMarkerAt(event->pos());
        if (marker_id != -1)
            selectMarker(marker_id);
        else
            addMarkerAtCurve(event->pos());
    }
    else
    {
        selectMarker(findMarkerAt(event->pos()));
    }
}

void MainWindow::handleMouseMove(QMouseEvent *event)
{
    if (!dragging)
        return;

    if (current_mode == InteractionMode::BoxZoom)
    {
        showBoxSelection(event->pos());
    }
    else if (current_mode == InteractionMode::ConstrainedZoom)
    {
        showXSelection(event->pos());
    }
}

void MainWindow::handleMouseRelease(QMouseEvent *event)
{
    if (!dragging)
        return;
    dragging = false;

    if (current_mode == InteractionMode::BoxZoom)
    {
        clearSelectionRect();

        double x1 = plot->xAxis->pixelToCoord(drag_start.x());
        double y1 = plot->yAxis->pixelToCoord(drag_start.y());
        double x2 = plot->xAxis->pixelToCoord(event->pos().x());
        double y2 = plot->yAxis->pixelToCoord(event->pos().y());

        if (std::abs(x2 - x1) > 1 && std::abs(y2 - y1) > 1)
        {
            saveZoomState();
            plot->xAxis->setRange(std::min(x1, x2), std::max(x1, x2));
            plot->yAxis->setRange(std::min(y1, y2), std::max(y1, y2));
            updateMarkerLabelPositions();
            plot->replot();
        }
    }
    else if (current_mode == InteractionMode::ConstrainedZoom)
    {
        clearSelectionRect();

        double x1 = plot->xAxis->pixelToCoord(drag_start.x());
        double x2 = plot->xAxis->pixelToCoord(event->pos().x());

        if (std::abs(event->pos().x() - drag_start.x()) > 3 && std::abs(x2 - x1) > 0.0)
        {
            saveZoomState();
            plot->xAxis->setRange(std::min(x1, x2), std::max(x1, x2));
            updateMarkerLabelPositions();
            plot->replot();
        }
    }
}

void MainWindow::clearSelectionRect()
{
    for (int i = 0; i < 4; ++i)
    {
        if (selection_rect[i])
        {
            plot->removeItem(selection_rect[i]);
            selection_rect[i] = nullptr;
        }
    }
}

void MainWindow::showBoxSelection(QPoint current_pos)
{
    clearSelectionRect();

    int x1 = drag_start.x(), y1 = drag_start.y();
    int x2 = current_pos.x(), y2 = current_pos.y();

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
}

void MainWindow::showXSelection(QPoint current_pos)
{
    clearSelectionRect();

    double x1 = plot->xAxis->pixelToCoord(drag_start.x());
    double x2 = plot->xAxis->pixelToCoord(current_pos.x());
    double y1 = plot->yAxis->range().lower;
    double y2 = plot->yAxis->range().upper;

    selection_rect[0] = new QCPItemLine(plot);
    selection_rect[0]->start->setCoords(x1, y1);
    selection_rect[0]->end->setCoords(x2, y1);
    selection_rect[0]->setPen(QPen(Qt::gray, 1, Qt::DashLine));

    selection_rect[1] = new QCPItemLine(plot);
    selection_rect[1]->start->setCoords(x2, y1);
    selection_rect[1]->end->setCoords(x2, y2);
    selection_rect[1]->setPen(QPen(Qt::gray, 1, Qt::DashLine));

    selection_rect[2] = new QCPItemLine(plot);
    selection_rect[2]->start->setCoords(x2, y2);
    selection_rect[2]->end->setCoords(x1, y2);
    selection_rect[2]->setPen(QPen(Qt::gray, 1, Qt::DashLine));

    selection_rect[3] = new QCPItemLine(plot);
    selection_rect[3]->start->setCoords(x1, y2);
    selection_rect[3]->end->setCoords(x1, y1);
    selection_rect[3]->setPen(QPen(Qt::gray, 1, Qt::DashLine));

    plot->replot();
}

void MainWindow::addMarkerAtCurve(QPoint pixel_pos)
{
    if (!current_data)
        return;

    int best_curve = -1;
    int best_idx = -1;
    double best_dist = 10.0;

    for (int curve = 0; curve < plot->graphCount(); ++curve)
    {
        if (!isGraphAllowedForMarker(current_marker_type, curve))
            continue;

        int idx;
        double dist = findNearestPointOnCurve(curve, pixel_pos, idx);
        if (dist < best_dist)
        {
            best_dist = dist;
            best_curve = curve;
            best_idx = idx;
        }
    }

    if (best_curve == -1)
        return;

    QCPGraph *graph = plot->graph(best_curve);
    double x = graph->data()->at(best_idx)->key;
    double y = graph->data()->at(best_idx)->value;

    QColor color = markerTypeColor(current_marker_type);

    QCPItemTracer *point = new QCPItemTracer(plot);
    point->setGraph(graph);
    point->setGraphKey(x);
    point->setInterpolating(false);
    point->setStyle(QCPItemTracer::tsCircle);
    point->setSize(6);
    point->setPen(QPen(color, 1));
    point->setBrush(QColor(120, 220, 255));
    point->position->setCoords(x, y);
    point->updatePosition();

    QCPItemText *label = new QCPItemText(plot);
    label->setColor(color);
    label->setPositionAlignment(Qt::AlignRight | Qt::AlignBottom);
    label->position->setType(QCPItemPosition::ptAbsolute);
    label->setText(markerLabelText(current_marker_type, x, y));
    label->setClipToAxisRect(false);

    MarkerData marker;
    marker.x = x;
    marker.y = y;
    marker.curve_index = best_curve;
    marker.point_index = best_idx;
    marker.type = current_marker_type;
    marker.point = point;
    marker.label = label;

    int marker_id = next_marker_id++;
    markers[marker_id] = marker;
    updateMarkerLabelPosition(marker_id);
    selectMarker(marker_id);

    plot->replot();
}

int MainWindow::findMarkerAt(QPoint pixel_pos) const
{
    int best_marker = -1;
    double best_distance = 12.0;

    for (auto it = markers.constBegin(); it != markers.constEnd(); ++it)
    {
        const MarkerData &marker = it.value();
        QPoint marker_pos(plot->xAxis->coordToPixel(marker.x), plot->yAxis->coordToPixel(marker.y));
        double distance = std::hypot(marker_pos.x() - pixel_pos.x(), marker_pos.y() - pixel_pos.y());
        if (distance < best_distance)
        {
            best_distance = distance;
            best_marker = it.key();
        }
    }

    return best_marker;
}

int MainWindow::findNearestPointByKey(int curve_idx, double x) const
{
    QCPGraph *graph = plot->graph(curve_idx);
    if (!graph || graph->data()->size() == 0)
        return -1;

    double best_distance = std::numeric_limits<double>::max();
    int best_idx = 0;
    for (int i = 0; i < graph->data()->size(); ++i)
    {
        double distance = std::abs(graph->data()->at(i)->key - x);
        if (distance < best_distance)
        {
            best_distance = distance;
            best_idx = i;
        }
    }

    return best_idx;
}

void MainWindow::selectMarker(int marker_id)
{
    selected_marker_id = markers.contains(marker_id) ? marker_id : -1;

    for (auto it = markers.constBegin(); it != markers.constEnd(); ++it)
    {
        updateMarkerVisual(it.key());
    }

    plot->replot();
}

void MainWindow::moveSelectedMarker(int key)
{
    if (!markers.contains(selected_marker_id))
        return;

    const MarkerData marker = markers[selected_marker_id];
    QCPGraph *graph = plot->graph(marker.curve_index);
    if (!graph || graph->data()->size() == 0 ||
        !isGraphAllowedForMarker(marker.type, marker.curve_index))
        return;

    int new_curve = marker.curve_index;
    int new_index = marker.point_index;

    if (key == Qt::Key_Left)
    {
        new_index = std::max(0, marker.point_index - 1);
    }
    else if (key == Qt::Key_Right)
    {
        new_index = std::min(graph->data()->size() - 1, marker.point_index + 1);
    }
    else if (key == Qt::Key_Up || key == Qt::Key_Down)
    {
        double current_pixel_y = plot->yAxis->coordToPixel(marker.y);
        double best_delta = std::numeric_limits<double>::max();

        if (marker.type != MarkerType::Generic)
            return;

        for (int curve = 0; curve < plot->graphCount(); ++curve)
        {
            QCPGraph *candidate_graph = plot->graph(curve);
            if (!candidate_graph || !isGraphAllowedForMarker(marker.type, curve) || curve == marker.curve_index)
                continue;

            int candidate_index = findNearestPointByKey(curve, marker.x);
            if (candidate_index == -1)
                continue;

            double candidate_y = candidate_graph->data()->at(candidate_index)->value;
            double candidate_pixel_y = plot->yAxis->coordToPixel(candidate_y);
            double delta = candidate_pixel_y - current_pixel_y;
            bool is_requested_direction = (key == Qt::Key_Up) ? delta < 0 : delta > 0;
            if (is_requested_direction && std::abs(delta) < best_delta)
            {
                best_delta = std::abs(delta);
                new_curve = curve;
                new_index = candidate_index;
            }
        }
    }

    updateMarkerPosition(selected_marker_id, new_curve, new_index);
    plot->replot();
}

void MainWindow::updateMarkerPosition(int marker_id, int curve_idx, int point_idx)
{
    if (!markers.contains(marker_id))
        return;

    MarkerData &marker = markers[marker_id];
    QCPGraph *graph = plot->graph(curve_idx);
    if (!graph || !isGraphAllowedForMarker(marker.type, curve_idx) || graph->data()->size() == 0)
        return;

    point_idx = std::clamp(point_idx, 0, graph->data()->size() - 1);
    double x = graph->data()->at(point_idx)->key;
    double y = graph->data()->at(point_idx)->value;

    marker.x = x;
    marker.y = y;
    marker.curve_index = curve_idx;
    marker.point_index = point_idx;

    marker.point->setGraph(graph);
    marker.point->setGraphKey(x);
    marker.point->position->setCoords(x, y);
    marker.point->updatePosition();

    marker.label->setColor(markerTypeColor(marker.type));
    marker.label->setText(markerLabelText(marker.type, x, y));
    updateMarkerLabelPosition(marker_id);

    updateMarkerVisual(marker_id);
}

void MainWindow::updateMarkerLabelPosition(int marker_id)
{
    if (!markers.contains(marker_id))
        return;

    MarkerData &marker = markers[marker_id];
    double marker_x = plot->xAxis->coordToPixel(marker.x);
    double marker_y = plot->yAxis->coordToPixel(marker.y);
    marker.label->position->setCoords(marker_x - 10.0, marker_y - 10.0);
}

void MainWindow::updateMarkerLabelPositions()
{
    for (auto it = markers.constBegin(); it != markers.constEnd(); ++it)
    {
        updateMarkerLabelPosition(it.key());
    }
}

void MainWindow::updateMarkerVisual(int marker_id)
{
    if (!markers.contains(marker_id))
        return;

    MarkerData &marker = markers[marker_id];
    QColor color = markerTypeColor(marker.type);
    bool selected = marker_id == selected_marker_id;

    marker.point->setSize(selected ? 8 : 6);
    marker.point->setPen(selected ? QPen(Qt::black, 1) : QPen(color, 1));
    marker.point->setBrush(QBrush(color.lighter(selected ? 145 : 125)));
    marker.label->setColor(selected ? Qt::black : color);
}

double MainWindow::findNearestPointOnCurve(int curve_idx, QPoint pixel_pos, int &point_idx)
{
    QCPGraph *graph = plot->graph(curve_idx);
    if (!graph || graph->data()->size() == 0)
        return 1e10;

    double min_dist = 1e10;
    int best_idx = 0;

    for (int i = 0; i < graph->data()->size(); ++i)
    {
        double x = graph->data()->at(i)->key;
        double y = graph->data()->at(i)->value;
        QPoint pt(plot->xAxis->coordToPixel(x), plot->yAxis->coordToPixel(y));

        double dist = (graph == target_graph)
                          ? std::abs(pt.x() - pixel_pos.x())
                          : std::hypot(pt.x() - pixel_pos.x(), pt.y() - pixel_pos.y());
        if (dist < min_dist)
        {
            min_dist = dist;
            best_idx = i;
        }
    }

    point_idx = best_idx;
    return min_dist;
}

void MainWindow::deleteMarker(int id)
{
    if (markers.contains(id))
    {
        if (selected_marker_id == id)
            selected_marker_id = -1;

        plot->removeItem(markers[id].point);
        plot->removeItem(markers[id].label);
        markers.remove(id);
        plot->replot();
    }
}
