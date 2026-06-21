#include "mainwindow.h"
#include "parquet_reader.h"
#include "qcustomplot.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QLabel>
#include <QMouseEvent>
#include <algorithm>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent) {
    setWindowTitle("Parquet ECG Viewer");
    setGeometry(100, 100, 1400, 800);

    setupUI();
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
    plot->setRenderHint(QCPPainter::rpHinted);
    main_layout->addWidget(plot);

    central->setLayout(main_layout);
}

void MainWindow::openFile() {
    QString filename = QFileDialog::getOpenFileName(this, "Open Parquet File", "", "Parquet Files (*.parquet)");
    if (filename.isEmpty()) return;

    try {
        auto data = ParquetReader::read(filename.toStdString());
        setupPlot(data);
    } catch (const std::exception& e) {
        qWarning() << "Error reading file:" << e.what();
    }
}

void MainWindow::setupPlot(const std::shared_ptr<ParquetData>& data) {
    plot->clearPlottables();
    plot->clearItems();

    QVector<double> x(data->raw_idx.begin(), data->raw_idx.end());

    // ECG plot
    QVector<double> y_ecg(data->ecg1_rotated.begin(), data->ecg1_rotated.end());
    plot->addGraph();
    plot->graph(0)->setData(x, y_ecg);
    plot->graph(0)->setName("ECG1");
    plot->graph(0)->setLineStyle(QCPGraph::lsLine);
    plot->graph(0)->setPen(QPen(Qt::blue, 1));

    // Pulse plot
    QVector<double> y_pulse(data->puls_raw.begin(), data->puls_raw.end());
    plot->addGraph();
    plot->graph(1)->setData(x, y_pulse);
    plot->graph(1)->setName("Pulse");
    plot->graph(1)->setLineStyle(QCPGraph::lsLine);
    plot->graph(1)->setPen(QPen(Qt::green, 1));

    // Find min value for target line base
    double min_val = *std::min_element(y_ecg.begin(), y_ecg.end());
    double max_val = *std::max_element(y_ecg.begin(), y_ecg.end());
    double offset = (max_val - min_val) * 0.05; // 5% above ECG value

    // Add vertical dashed lines for target=1
    for (size_t i = 0; i < data->target.size(); ++i) {
        if (data->target[i] == 1) {
            double x_pos = data->raw_idx[i];
            double y_ecg_val = data->ecg1_rotated[i];
            double y_top = y_ecg_val + offset;

            QCPItemLine* line = new QCPItemLine(plot);
            line->start->setCoords(x_pos, y_ecg_val);
            line->end->setCoords(x_pos, y_top);
            line->setPen(QPen(Qt::red, 1.5, Qt::DashLine));
            line->setPen(line->pen()); // refresh
            QColor dashColor = Qt::red;
            dashColor.setAlpha(120);
            line->setPen(QPen(dashColor, 1.5, Qt::DashLine));
        }
    }

    plot->xAxis->setAutoTickStep(false);
    plot->xAxis->setTickStep(1000);
    plot->rescaleAxes();
    plot->legend->setVisible(true);
    plot->replot();
}

