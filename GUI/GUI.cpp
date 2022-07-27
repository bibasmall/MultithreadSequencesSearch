#include <QtConcurrentRun>
#include <QMessageBox>
#include <QlistWidget>
#include <QToolButton>
#include <QFileDialog>
#include <qfilesystemmodel.h>
#include <qtextbrowser.h>
#include <string>
#include "GUI.h"

GUI::GUI(QWidget* parent) : QMainWindow(parent)
{
    ui.setupUi(this);

    ui.progressBar->setVisible(false);
    ui.SpinBoxSlice->setValue(8192);
    ui.pushButtonSave->setDisabled(true);

    connect(ui.listWidgetHex, &QListWidget::itemDoubleClicked, [this](QListWidgetItem* item) {
        this->current_item = item;
        this->current_item_text = item->text();
        ui.listWidgetHex->openPersistentEditor(item);
        this->ui.buttonExecute->setDisabled(true);
    });
    connect(ui.listWidgetHex, &QListWidget::itemSelectionChanged, [this]() {
    
        if (this->current_item && !this->current_item->isSelected())
        {
            ui.listWidgetHex->closePersistentEditor(this->current_item);
            if (current_item->text() != this->current_item_text)
            {
                if (this->seqtype.at(ui.listWidgetHex->row(current_item)) == SequenceType::IsHex)
                {
                    if (!RawBytes::make_hex(current_item->text().toStdString()))
                    {
                        this->current_item->setText(this->current_item_text);
                    }
                    else
                    {
                        this->current_item_text = current_item->text();
                        current_item->setToolTip(current_item_text + "  (hex)");
                    }
                }
                else
                {
                    this->current_item_text = current_item->text();
                    current_item->setToolTip(current_item_text);
                }
            }
        }
        this->ui.buttonExecute->setEnabled(true);
    });
    connect(ui.listWidgetHex, &QListWidget::itemClicked, [this](QListWidgetItem* ptr) {

        if (ui.listWidgetHex->isPersistentEditorOpen(ptr))
        {
            ui.listWidgetHex->closePersistentEditor(ptr);
            if (current_item->text() != this->current_item_text)
            {
                if (this->seqtype.at(ui.listWidgetHex->row(current_item)) == SequenceType::IsHex)
                {
                    if (!RawBytes::make_hex(current_item->text().toStdString()))
                    {
                        this->current_item->setText(this->current_item_text);
                    }
                    else
                    {
                        this->current_item_text = current_item->text();
                        current_item->setToolTip(current_item_text + "  (hex)");
                    }
                }
                else
                {
                    this->current_item_text = current_item->text();
                    current_item->setToolTip(current_item_text);
                }
            }
        }
        this->ui.buttonExecute->setEnabled(true);
    });
    connect(ui.listWidgetHex, &QListWidget::customContextMenuRequested, [this](const QPoint& point) {
        provideQListWidgetContextMenu(point, ui.listWidgetHex);
    });
    connect(ui.treeWidgetPaths, &QListWidget::customContextMenuRequested, [this](const QPoint& point) {
        provideQTreeWidgetContextMenu(point, ui.treeWidgetPaths);
    });
    connect(ui.lineEditHex, &QLineEdit::returnPressed, [this]() {
        if (ui.lineEditHex->text().isEmpty())
            return;
        if (ui.checkBoxIsHex->isChecked())
        {
            auto ptr = RawBytes::make_hex(ui.lineEditHex->text().toStdString());
            if (ptr)
            {
                ui.listWidgetHex->addItem(ui.lineEditHex->text());
                ui.listWidgetHex->item(ui.listWidgetHex->count() - 1)->setToolTip(ui.listWidgetHex->item(ui.listWidgetHex->count() - 1)->text() + "  (hex)");
                this->seqtype.push_back(SequenceType::IsHex);
                ui.lineEditHex->clear();
            }
            else
                showMessageWindow(ui.cyrillicLabel3->text());
        }
        else
        {
            ui.listWidgetHex->addItem(ui.lineEditHex->text());
            ui.listWidgetHex->item(ui.listWidgetHex->count() - 1)->setToolTip(ui.listWidgetHex->item(ui.listWidgetHex->count() - 1)->text());
            this->seqtype.push_back(SequenceType::IsText);
            ui.lineEditHex->clear();
        }
    });
    connect(ui.buttonAddDirectory, &QPushButton::clicked, [this]() {
        
        auto qdirpath = QFileDialog::getExistingDirectory(this, {});
        if (!qdirpath.isEmpty())
        {
            QTreeWidgetItem* item_dir = new QTreeWidgetItem(ui.treeWidgetPaths);
            item_dir->setText(0, qdirpath);
            QDirIterator it(qdirpath, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext())
            {
                QTreeWidgetItem* item_file = new QTreeWidgetItem(item_dir);
                item_file->setText(1, it.next());
            }
        }
    });
    connect(ui.buttonAddFile, &QPushButton::clicked, [this]() {
        
        auto qfilepath = QFileDialog::getOpenFileName(this, {});
        if (!qfilepath.isEmpty())
        {
            QTreeWidgetItem* item_dir = new QTreeWidgetItem(ui.treeWidgetPaths);
            item_dir->setText(0, QString::fromStdWString(std::filesystem::path((qfilepath.toStdWString())).parent_path().wstring()));
            QTreeWidgetItem* item_file = new QTreeWidgetItem(item_dir);
            item_file->setText(1, qfilepath);

            ui.treeWidgetPaths->addTopLevelItem(item_dir);
        }        
    });
    connect(ui.buttonExecute, &QPushButton::clicked, [this]() {this->exec(ui.SpinBoxSlice->value()); });
    connect(ui.pushButtonSave, &QPushButton::clicked, this, &GUI::saveButtonClicked);
}

void GUI::showMessageWindow(const QString& msg)
{
    QMessageBox box;
    box.setText(msg);
    box.setDefaultButton(QMessageBox::Ok);
    box.exec();
}
void GUI::provideQListWidgetContextMenu(const QPoint& pos, QListWidget* ptr)
{
    QMenu submenu;
    submenu.addAction(ui.cyrillicLabel2->text(), [ptr, this]() {
        this->seqtype.erase(std::next(this->seqtype.begin(), ptr->row(ptr->currentItem())));
        qDeleteAll(ptr->selectedItems());});
    submenu.exec(ptr->mapToGlobal(pos));
}
void GUI::provideQTreeWidgetContextMenu(const QPoint& pos, QTreeWidget* ptr)
{
    QMenu submenu;
    auto deleteAction = submenu.addAction(ui.cyrillicLabel2->text(), [ptr, pos]() {
        auto item_ptr = ptr->itemAt(pos);
        auto item_parent_ptr = item_ptr->parent();
        if (item_parent_ptr != nullptr)
        {
            item_parent_ptr->removeChild(item_ptr);
            if (item_parent_ptr->childCount() == 0)
                delete item_parent_ptr;
        }
        delete item_ptr;
    });
    auto showSearchResultAction = submenu.addAction(ui.cyrillicLabel4->text(), [ptr, pos, this]() {
        auto item_ptr = ptr->itemAt(pos);
        auto item_parent_ptr = item_ptr->parent();
        if (item_parent_ptr != nullptr)
        {
            auto textbrows = new QTextBrowser{};
            textbrows->setWindowTitle(ui.cyrillicLabel10->text());
            textbrows->insertPlainText(printFileResult(item_ptr->text(1)));
            textbrows->resize(500, 500);
            textbrows->showNormal();
        }
    });
    if(this->res_data.empty())
        showSearchResultAction->setEnabled(false);
    submenu.exec(ptr->mapToGlobal(pos));
}
void GUI::exec(size_t slice)
{
    this->res_data.reset();
    this->search.reset();
    this->fillPaths();
    this->fillHexes();
    if (!this->search.ready())
        return;
    this->setWidgetsDisabled(true);
    ui.progressBar->setVisible(true);
    auto size_ = this->search.size();
    auto cb = [this](auto value) {ui.progressBar->setValue(value); };
    auto i = std::atomic<unsigned>(0);
    auto t = QtConcurrent::run([this, slice, cb, &i] { return this->search.exec_and_reset(slice, i); });
    while (!t.isFinished())
    {
        auto res = (i.load() * 100) / size_;
        ui.progressBar->setValue(res);
        QApplication::processEvents();
    }

    this->res_data = t.takeResult();
    ui.progressBar->setValue(0);
    ui.progressBar->setVisible(false);
    this->setWidgetsDisabled(false);
}

void GUI::saveButtonClicked()
{
    auto filedialog = new QFileDialog{ this };
    filedialog->setDefaultSuffix("txt");
    QFile file(filedialog->getSaveFileName(this, {}, "*.txt", "text file (*.txt)"));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;
    QTextStream out(&file);

    for (QTreeWidgetItemIterator it{ ui.treeWidgetPaths }; *it; ++it)
    {
        for (int i = 0; i != (*it)->childCount(); ++i)
            out << printFileResult((*it)->child(i)->text(1));
        
    }

    file.close();

}

void GUI::fillPaths() noexcept
{
    try
    {
        for (QTreeWidgetItemIterator it{ ui.treeWidgetPaths }; *it; ++it)
        {
            for (int i = 0; i != (*it)->childCount(); ++i)
            {
                auto wstr = (*it)->child(i)->text(1).toStdWString();
                this->search.add_path((*it)->child(i)->text(1).toStdWString());
            }
        }
    }
    catch (const std::exception& e)
    {
        qDebug() << e.what();
    }
    catch (...)
    {
        qDebug() << "unexpected err";
    }

}
void GUI::fillHexes() noexcept
{
    try
    {
        for (int i = 0; i != ui.listWidgetHex->count(); ++i)
        {
            if (this->seqtype.at(i) == SequenceType::Unknown)
                throw std::logic_error("Undefined behavior");

            if (this->seqtype.at(i) == SequenceType::IsHex)
                this->search.add_bytes(RawBytes::make_hex(ui.listWidgetHex->item(i)->text().toStdString()).value());
            else
                this->search.add_bytes(RawBytes{ ui.listWidgetHex->item(i)->text().toStdWString() });
        }
    }
    catch (const std::exception& e)
    {
        qDebug() << e.what();
    }
    catch (...)
    {
        qDebug() << "unexpected err";
    }
}
void GUI::setWidgetsDisabled(bool f) noexcept
{
    ui.buttonAddDirectory->setDisabled(f);
    ui.pushButtonSave->setDisabled(f);
    ui.buttonAddFile->setDisabled(f);
    ui.buttonExecute->setDisabled(f);
    ui.lineEditHex->setDisabled(f);
    ui.SpinBoxSlice->setDisabled(f);
    ui.listWidgetHex->setDisabled(f);
    ui.treeWidgetPaths->setDisabled(f);
    ui.checkBoxIsHex->setDisabled(f);
}
QString GUI::printFileResult(const QString& path)
{;
    QString qstr{};
    if (this->res_data.contains(path.toStdWString()))
    {
        qstr.append(ui.cyrillicLabel5->text()).append("\n").append(path).append("\n");
        for (int i = 0; i != ui.listWidgetHex->count(); ++i)
        {
            try
            {
                PositionsInFile positions;
                if (this->seqtype.at(i) == SequenceType::Unknown)
                    throw std::logic_error("Undefined behavior");

                if(this->seqtype.at(i) == SequenceType::IsHex)
                    positions = this->res_data.at(path.toStdWString(), RawBytes::make_hex(ui.listWidgetHex->item(i)->text().toStdString()).value());
                else
                {
                    auto check = ui.listWidgetHex->item(i)->text().toStdWString();
                    positions = this->res_data.at(path.toStdWString(), RawBytes{ check });
                }
                qstr.append(ui.cyrillicLabel6->text()).append(" ").append(ui.listWidgetHex->item(i)->text()).append(" ");
                if (!positions.empty())
                {
                    qstr.append(ui.cyrillicLabel7->text()).append(" \n");
                    for (auto e : positions)
                        qstr.append(QString::number(e)).append("    ");
                }
                else
                    qstr.append(ui.cyrillicLabel8->text());
                qstr.append("\n\n");
            }
            catch (const std::exception& e)
            {
                qDebug() << e.what();
            }
            catch (...)
            {
                qDebug() << "unexpected err";
            }
        }
    }
    else
        qstr.append(path).append(" ").append(ui.cyrillicLabel9->text()).append("\n").append("\n");
    

    return qstr;
}
