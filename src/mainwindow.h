#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <memory>
#include <QMap>
#include <QPoint>
#include <QStack>

class QCustomPlot;
class ParquetData;
class QPushButton;
class QCPItemLine;
class QCPItemText;

enum class InteractionMode { Normal, BoxZoom, ConstrainedZoom, AddMarker };

struct ZoomState {
    double x_min, x_max, y_min, y_max;
};

struct MarkerData {
    double x;
    double y;
    int curve_index;
    QCPItemText* label;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void openFile();
    void setupPlot(const std::shared_ptr<ParquetData>& data);
    void setBoxZoomMode();
    void setConstrainedZoomMode();
    void setAddMarkerMode();
    void zoomIn();
    void zoomOut();
    void fitView();
    void undoZoom();
    void applyTargetHeight();
    void deleteMarker(int id);

private:
    QCustomPlot* plot;
    std::shared_ptr<ParquetData> current_data;
    InteractionMode current_mode = InteractionMode::Normal;

    double percentile_95 = 0.0;

    QMap<int, MarkerData> markers;
    int next_marker_id = 0;

    QStack<ZoomState> zoom_history;

    QPoint drag_start;
    QCPItemLine* selection_rect[4];
    bool dragging = false;
    QPoint last_mouse_pos;
    int constrain_axis = -1;

    QPushButton* box_zoom_btn;
    QPushButton* constrained_zoom_btn;
    QPushButton* add_marker_btn;
    QPushButton* undo_btn;
    class QDoubleSpinBox* target_height_spin;
    double target_height = 0.0;

    void setupUI();
    void setupToolbar();
    void updateModeButtons();
    void saveZoomState();
    void addMarkerAtCurve(QPoint pixel_pos);
    double findNearestPointOnCurve(int curve_idx, QPoint pixel_pos, int& point_idx);
    void handleMousePress(QMouseEvent* event);
    void handleMouseMove(QMouseEvent* event);
    void handleMouseRelease(QMouseEvent* event);
};

#endif // MAINWINDOW_H
