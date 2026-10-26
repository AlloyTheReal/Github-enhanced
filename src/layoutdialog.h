#pragma once
#include "visuals.h"
#include <QJsonObject>
class MainWindow;
class QDockWidget;
class QListWidget;
class QComboBox;
class QSpinBox;
class QWidget;
class LayoutCanvas;
class QSlider;
class QLabel;
class QPushButton;
class QTimer;

// Modeless: changes apply live while the real workspace remains interactive.
class LayoutDialog : public GlassDialog {
    Q_OBJECT
  public:
    explicit LayoutDialog(MainWindow *window);

  protected:
    void done(int result) override;

  private:
    MainWindow *window;
    QJsonObject original;
    QListWidget *panels;
    QListWidget *tools;
    QListWidget *profiles;
    QComboBox *position;
    QComboBox *relativeTo;
    QComboBox *placement;
    QSpinBox *panelWidth;
    QSpinBox *panelHeight;
    LayoutCanvas *preview;
    QSlider *widthSlider;
    QSlider *heightSlider;
    QLabel *widthLabel;
    QLabel *heightLabel;
    QPushButton *undoButton;
    QPushButton *redoButton;
    QTimer *resizeTimer;
    QList<QJsonObject> undoStates;
    QList<QJsonObject> redoStates;
    bool updating = false;
    QDockWidget *selectedPanel() const;
    void selectPanel();
    void refreshPanelList();
    void refreshTools();
    void applyTools();
    void refreshProfiles();
    void remember();
    void undo(bool redo = false);
    void updateDimensions();
};
