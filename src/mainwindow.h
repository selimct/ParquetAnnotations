#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <memory>

class QCustomPlot;
class ParquetData;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void openFile();
    void setupPlot(const std::shared_ptr<ParquetData>& data);

private:
    QCustomPlot* plot;

    void setupUI();
};

#endif // MAINWINDOW_H
