#pragma once
#include <QPlainTextEdit>
#include <QWidget>
class QLabel;
class QComboBox;
class QLineEdit;
class QStackedWidget;
class DiffEditor : public QPlainTextEdit {
public:
    explicit DiffEditor(QWidget *parent=nullptr);
    void setLines(const QString &text, const QStringList &numbers, const QList<int> &kinds);
    void paintGutter(QPaintEvent *event);
protected:
    void resizeEvent(QResizeEvent *event) override;
private:
    QWidget *gutter;
    QStringList numbers;
    int gutterWidth=96;
};
class DiffViewer : public QWidget {
public:
    explicit DiffViewer(QWidget *parent=nullptr);
    QPlainTextEdit *editor() const { return unified; }
    void setContent(const QString &text, bool patch=true);
    void refreshColors();
    void setFileOpen(bool open);
private:
    DiffEditor *unified, *before, *after;
    QLabel *statistics;
    QWidget *tools, *findTools;
    QComboBox *mode;
    QLineEdit *search;
    QStackedWidget *views;
    QList<int> hunks;
    QString content;
    bool isPatch=true;
    void jump(int direction);
};
