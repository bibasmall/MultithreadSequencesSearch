#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_GUI.h"
#include "HexCore.h"

class GUI : public QMainWindow
{
    Q_OBJECT

public:
    GUI(QWidget* parent = Q_NULLPTR);
    void showMessageWindow(const QString&);
    void provideQListWidgetContextMenu(const QPoint&, QListWidget*);
    void provideQTreeWidgetContextMenu(const QPoint&, QTreeWidget*);
    void displayResult(size_t);
    void exec(size_t);

public slots:
    void saveButtonClicked();

private:
    void fillPaths() noexcept;
    void fillHexes() noexcept;
    void setWidgetsDisabled(bool) noexcept;
    QString printFileResult(const QString&);

    enum class SequenceType {IsHex, IsText, Unknown};

private:
    Ui::GUIClass ui;
    QListWidgetItem* current_item{ nullptr };
    QString current_item_text{};
    Search search{};
    SearchRes res_data{};
    std::vector<SequenceType> seqtype{};
};
