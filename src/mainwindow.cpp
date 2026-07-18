#include "mainwindow.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QMessageBox>
#include <QMimeData>
#include <QPixmap>
#include <QSettings>
#include <QUrl>
#include <QVBoxLayout>

namespace {
const QSize kCoverSize(160, 160);

QSettings *appSettings() {
    static QSettings s(QSettings::IniFormat, QSettings::UserScope, "MetaBuddy", "MetaBuddy");
    return &s;
}
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("MetaBuddy");
    setAcceptDrops(true);
    resize(480, 560);

    m_stack = new QStackedWidget(this);
    setCentralWidget(m_stack);

    // Window 1 (drag n drop)
    auto *dropPage   = new QWidget;
    auto *dropLayout = new QVBoxLayout(dropPage);
    m_dropLabel      = new QLabel("Drag & drop an audio file here\n(mp3, flac, aiff, wav)");
    m_dropLabel->setAlignment(Qt::AlignCenter);
    m_dropLabel->setStyleSheet("QLabel { border: 2px dashed #888; border-radius: 12px; padding: 60px; "
                               "font-size: 15px; color: #555; }");
    dropLayout->addStretch();
    dropLayout->addWidget(m_dropLabel);

    auto *browseBtn = new QPushButton("...or choose a file");
    connect(browseBtn, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(this, "Choose an audio file", QString(),
                                                          "Audio files (*.mp3 *.flac *.aiff *.aif *.wav)");
        if (!path.isEmpty()) loadFile(path);
    });
    dropLayout->addWidget(browseBtn, 0, Qt::AlignCenter);
    dropLayout->addStretch();
    m_stack->addWidget(dropPage);

    // Window 2 (metadata editor)
    auto *editPage   = new QWidget;
    auto *editLayout = new QVBoxLayout(editPage);

    m_fileLabel = new QLabel;
    m_fileLabel->setStyleSheet("font-weight: bold;");
    m_fileLabel->setWordWrap(true);
    editLayout->addWidget(m_fileLabel);

    auto *coverRow = new QHBoxLayout();
    m_coverPreview = new QLabel;
    m_coverPreview->setFixedSize(kCoverSize);
    m_coverPreview->setAlignment(Qt::AlignCenter);
    m_coverPreview->setStyleSheet("border: 1px solid #999; background: #eee;");
    m_coverPreview->setText("No cover");
    coverRow->addWidget(m_coverPreview);

    auto *coverBtnCol = new QVBoxLayout();
    m_chooseCoverBtn  = new QPushButton("Choose Cover...");
    m_removeCoverBtn  = new QPushButton("Remove Cover");
    connect(m_chooseCoverBtn, &QPushButton::clicked, this, &MainWindow::chooseCover);
    connect(m_removeCoverBtn, &QPushButton::clicked, this, &MainWindow::removeCover);
    coverBtnCol->addWidget(m_chooseCoverBtn);
    coverBtnCol->addWidget(m_removeCoverBtn);

    m_coverWarningLabel = new QLabel;
    m_coverWarningLabel->setWordWrap(true);
    m_coverWarningLabel->setStyleSheet("color: #b06f00; font-size: 11px;");
    coverBtnCol->addWidget(m_coverWarningLabel);

    coverBtnCol->addStretch();
    coverRow->addLayout(coverBtnCol);
    coverRow->addStretch();
    editLayout->addLayout(coverRow);

    auto *form   = new QFormLayout();
    m_labelEdit  = new QLineEdit;
    m_artistEdit = new QLineEdit;
    m_titleEdit  = new QLineEdit;
    m_albumEdit  = new QLineEdit;
    m_genreEdit  = new QLineEdit;
    m_yearSpin   = new QSpinBox;
    m_yearSpin->setRange(0, 3000);
    m_yearSpin->setSpecialValueText(" ");
    // m_trackSpin = new QSpinBox;
    // m_trackSpin->setRange(0, 999);
    // m_trackSpin->setSpecialValueText(" ");
    m_commentEdit = new QPlainTextEdit;
    connect(m_labelEdit,   &QLineEdit::textChanged, this, &MainWindow::updateDirtyIndicators);
    connect(m_artistEdit,  &QLineEdit::textChanged, this, &MainWindow::updateDirtyIndicators);
    connect(m_titleEdit,   &QLineEdit::textChanged, this, &MainWindow::updateDirtyIndicators);
    connect(m_albumEdit,   &QLineEdit::textChanged, this, &MainWindow::updateDirtyIndicators);
    connect(m_genreEdit,   &QLineEdit::textChanged, this, &MainWindow::updateDirtyIndicators);
    connect(m_yearSpin,    QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::updateDirtyIndicators);
    // connect(m_trackSpin,   QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::updateDirtyIndicators);
    connect(m_commentEdit, &QPlainTextEdit::textChanged, this, &MainWindow::updateDirtyIndicators);
    m_commentEdit->setFixedHeight(60);

    form->addRow("Label:", makeLockableRow(m_labelEdit, "label"));
    form->addRow("Artist:", makeLockableRow(m_artistEdit, "artist"));
    form->addRow("Title:", makeLockableRow(m_titleEdit, "title"));
    form->addRow("Album:", makeLockableRow(m_albumEdit, "album"));
    form->addRow("Genre:", makeLockableRow(m_genreEdit, "genre"));
    form->addRow("Year:", makeLockableRow(m_yearSpin, "year"));
    // form->addRow("Track #:", makeLockableRow(m_trackSpin, "track"));
    form->addRow("Comment:", makeLockableRow(m_commentEdit, "comment"));
    editLayout->addLayout(form);

    m_statusLabel = new QLabel;
    m_statusLabel->setWordWrap(true);
    editLayout->addWidget(m_statusLabel);

    auto *btnRow     = new QHBoxLayout();
    m_saveBtn        = new QPushButton("Save (Overwrite)");
    m_saveCopyBtn    = new QPushButton("Save As Copy...");
    m_anotherFileBtn = new QPushButton("Load Another File");
    connect(m_saveBtn, &QPushButton::clicked, this, &MainWindow::saveOverwrite);
    connect(m_saveCopyBtn, &QPushButton::clicked, this, &MainWindow::saveAsCopy);
    connect(m_anotherFileBtn, &QPushButton::clicked, this, &MainWindow::loadAnotherFile);
    btnRow->addWidget(m_saveBtn);
    btnRow->addWidget(m_saveCopyBtn);
    editLayout->addLayout(btnRow);
    editLayout->addWidget(m_anotherFileBtn);

    m_stack->addWidget(editPage);
    m_stack->setCurrentIndex(0);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event) {
    const auto urls = event->mimeData()->urls();
    if (urls.isEmpty()) return;
    const QString path = urls.first().toLocalFile();
    if (path.isEmpty()) return;

    static const QStringList imageExts =  {"png","jpg","jpeg"};
    const QString ext = QFileInfo(path).suffix().toLower();

    if (imageExts.contains(ext)) {
        if (m_currentFile.isEmpty()) {
            QMessageBox::information(this, "Load a track first.",
                "Drop an audio file first, then drag a cover image.");
                return;
        }
        loadCoverImage(path);
        return;
    }
    loadFile(path);
    // if (!path.isEmpty()) loadFile(path);
}

void MainWindow::loadFile(const QString &path) {
    if (!QFile::exists(path)) {
        QMessageBox::warning(this, "File not found", "That file could not be found.");
        return;
    }
    if (!TagIO::isSupported(path)) {
        QMessageBox::warning(this, "Unsupported file",
                             "This file type isn't supported yet.\n"
                             "Supported: mp3, flac, aiff/aif, wav.");
        return;
    }

    TrackMetadata meta;
    if (!TagIO::readMetadata(path, meta)) {
        QMessageBox::warning(this, "Couldn't read file", "This file's tags could not be read.");
        return;
    }

    m_currentFile = path;
    m_current     = meta;

    if (m_current.title.isEmpty()) m_current.title = QFileInfo(path).completeBaseName();

    m_savedBaseline = m_current;

    if (m_current.year <= 0)
        m_current.year = QDate::currentDate().year(); // default to current year


    applyLockedValues();
    applyMetadataToForm(m_current);

    m_fileLabel->setText(QFileInfo(path).fileName());
    m_statusLabel->clear();
    updateDirtyIndicators();
    m_stack->setCurrentIndex(1);
}

QWidget *MainWindow::makeLockableRow(QWidget *editor, const QString &key) {
    auto *container = new QWidget;
    auto *layout    = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    auto *lockBtn = new QToolButton;
    lockBtn->setCheckable(true);
    lockBtn->setFixedSize(26, 26);
    lockBtn->setToolTip("Lock this field's value for reusing it.");

    const bool locked = appSettings()->value("locked/" + key, false).toBool();
    lockBtn->setChecked(locked);
    lockBtn->setText(locked ? "🔒" : "");
    editor->setEnabled(!locked); // turn off edit the row

    connect(lockBtn, &QToolButton::toggled, this, [editor, lockBtn, key](bool checked) {
        appSettings()->setValue("locked/" + key, checked);
        lockBtn->setText(checked ? "🔒" : "");
        editor->setEnabled(!checked);
    });

    m_lockButtons[key] = lockBtn;
    layout->addWidget(editor, 1);
    layout->addWidget(lockBtn);
    return container;
}

void MainWindow::applyLockedValues() {
    QSettings *s      = appSettings();
    auto       locked = [&](const QString &key) { return m_lockButtons[key]->isChecked(); };

    if (locked("label")) m_current.label     = s->value("value/label").toString();
    if (locked("artist")) m_current.artist   = s->value("value/artist").toString();
    if (locked("title")) m_current.title     = s->value("value/title").toString();
    if (locked("album")) m_current.album     = s->value("value/album").toString();
    if (locked("genre")) m_current.genre     = s->value("value/genre").toString();
    if (locked("comment")) m_current.comment = s->value("value/comment").toString();
    if (locked("year")) {
        const int storedYear = s->value("value/year", 0).toInt();
        if (storedYear > 0) m_current.year = storedYear;
    }
    // if (locked("track")) m_current.track     = s->value("value/track").toInt();
}

void MainWindow::persistLockedValues(const TrackMetadata &m) {
    QSettings *s      = appSettings();
    auto       locked = [&](const QString &key) { return m_lockButtons[key]->isChecked(); };

    if (locked("label")) s->setValue("value/label", m.label);
    if (locked("artist")) s->setValue("value/artist", m.artist);
    if (locked("title")) s->setValue("value/title", m.title);
    if (locked("album")) s->setValue("value/album", m.album);
    if (locked("genre")) s->setValue("value/genre", m.genre);
    if (locked("comment")) s->setValue("value/comment", m.comment);
    if (locked("year")) s->setValue("value/year", m.year);
    // if (locked("track")) s->setValue("value/track", m.track);
}

void MainWindow::applyMetadataToForm(const TrackMetadata &m) {
    m_labelEdit->setText(m.label);
    m_artistEdit->setText(m.artist);
    m_titleEdit->setText(m.title);
    m_albumEdit->setText(m.album);
    m_genreEdit->setText(m.genre);
    m_yearSpin->setValue(m.year);
    // m_trackSpin->setValue(m.track);
    m_commentEdit->setPlainText(m.comment);
    updateCoverPreview();
}

TrackMetadata MainWindow::collectFormData() const {
    TrackMetadata m = m_current;
    m.label         = m_labelEdit->text();
    m.artist        = m_artistEdit->text();
    m.title         = m_titleEdit->text();
    m.album         = m_albumEdit->text();
    m.genre         = m_genreEdit->text();
    m.year          = m_yearSpin->value();
    // m.track         = m_trackSpin->value();
    m.comment       = m_commentEdit->toPlainText();
    return m;
}

void MainWindow::updateDirtyIndicators() {
    auto mark = [](QWidget *w, bool dirty) {
        w->setStyleSheet(dirty ? "background-color: #fff3cd; color: #000000" : "");
    };
    mark(m_labelEdit,    m_labelEdit->text()            != m_savedBaseline.label);
    mark(m_artistEdit,   m_artistEdit->text()           != m_savedBaseline.artist);
    mark(m_titleEdit,    m_titleEdit->text()            != m_savedBaseline.title);
    mark(m_albumEdit,    m_albumEdit->text()            != m_savedBaseline.album);
    mark(m_genreEdit,    m_genreEdit->text()            != m_savedBaseline.genre);
    mark(m_yearSpin,     m_yearSpin->value()            != m_savedBaseline.year);
    // mark(m_trackSpin,    m_trackSpin->value()           != m_savedBaseline.track);
    mark(m_commentEdit,  m_commentEdit->toPlainText()   != m_savedBaseline.comment);
}

void MainWindow::updateCoverPreview() {
    if (m_current.hasCover && !m_current.coverData.isEmpty()) {
        QImage image;
        image.loadFromData(m_current.coverData);
        if (!image.isNull()) {
            m_coverPreview->setPixmap(
                QPixmap::fromImage(image).scaled(kCoverSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            m_coverWarningLabel->setText(
                (image.width() < 3000 || image.height() < 3000)
                    ? "Recommended cover art size is 3000x3000"
                    : QString());
            return;
        }
    }
    m_coverPreview->setPixmap(QPixmap());
    m_coverPreview->setText("No cover");
    m_coverWarningLabel->clear();
}

void MainWindow::chooseCover() {
    const QString path = QFileDialog::getOpenFileName(
        this, "Choose cover image", QString(), "Images (*.jpg *.jpeg *.png)");
    if (!path.isEmpty()) loadCoverImage(path);
}

void MainWindow::loadCoverImage(const QString &path){
    QImageReader reader(path);
    const QByteArray format = reader.format().toLower();

    static const QSet<QByteArray> supported = {"png", "jpeg", "jpg"};
    if (!supported.contains(format)){
        QMessageBox::warning(this, "Unsupported format",
            "Cover art must be a PNG or JPEG/JPG");
        return;
    }

    const QImage image = reader.read();
    if (image.isNull()) {
        QMessageBox::warning(this, "Couldn't read image", "That file could not be opened.");
        return;
    }
    if (image.width() != image.height()) {
        QMessageBox::warning(this, "Cover art must be square",
            QString("This image is %1x%2. Cover art must be 1:1 ratio")
                .arg(image.width()).arg(image.height()));
        return;
    }

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Couldn't read image", "That image file could not be opened.");
        return;
    }
    m_current.coverData = f.readAll();
    m_current.coverMime = (format == "png") ? "image/png" : "image/jpeg";
    m_current.hasCover = true;
    updateCoverPreview();
}

void MainWindow::removeCover() {
    m_current.hasCover = false;
    m_current.coverData.clear();
    m_current.coverMime.clear();
    updateCoverPreview();
}

void MainWindow::setStatus(const QString &text, bool isError) {
    m_statusLabel->setStyleSheet(isError ? "color: #b00020;" : "color: #1b7a1b;");
    m_statusLabel->setText(text);
}

void MainWindow::saveOverwrite() {
    m_current = collectFormData();
    persistLockedValues(m_current);

    if (!TagIO::writeMetadata(m_currentFile, m_current)) {
        setStatus("Failed to save metadata to the file.", true);
        return;
    }
    m_savedBaseline = m_current;
    updateDirtyIndicators();
    setStatus("Saved: " + QFileInfo(m_currentFile).fileName());
}

void MainWindow::saveAsCopy() {
    m_current = collectFormData();
    persistLockedValues(m_current);

    const QFileInfo info(m_currentFile);
    const QString   suggested = info.absolutePath() + "/" + info.completeBaseName() + "_tagged." + info.suffix();
    const QString   destPath  = QFileDialog::getSaveFileName(this, "Save copy as", suggested, "*." + info.suffix());
    if (destPath.isEmpty()) return;

    QFile::remove(destPath); // QFile::copy fails if the destination exists
    if (!QFile::copy(m_currentFile, destPath)) {
        setStatus("Failed to create the copy.", true);
        return;
    }
    if (!TagIO::writeMetadata(destPath, m_current)) {
        setStatus("Copy created, but writing metadata failed.", true);
        return;
    }
    setStatus("Saved copy: " + QFileInfo(destPath).fileName());
}

void MainWindow::loadAnotherFile() {
    m_currentFile.clear();
    m_current = TrackMetadata();
    m_stack->setCurrentIndex(0);
}
