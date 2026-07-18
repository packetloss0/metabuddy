#pragma once

#include <QMainWindow>
#include <QLineEdit>
#include <QSpinBox>
#include <QPlainTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QStackedWidget>
#include <QToolButton>
#include <QMap>
#include <QImageReader>

#include "tagio.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void chooseCover();
    void removeCover();
    void saveOverwrite();
    void saveAsCopy();
    void loadAnotherFile();

private:
    void loadFile(const QString &path);
    
    void applyMetadataToForm(const TrackMetadata &m);
    TrackMetadata collectFormData() const;
    TrackMetadata m_savedBaseline;
    void updateDirtyIndicators();
    
    void updateCoverPreview();
    //void applyPersistentDefaults();
    //void savePersistentDefaults();
    void setStatus(const QString &text, bool isError = false);
    void loadCoverImage(const QString &path);

    QString m_currentFile;
    TrackMetadata m_current;

    QStackedWidget *m_stack;

    // Page 0: drop target
    QLabel *m_dropLabel;

    // Page 1: editor form
    QLabel *m_fileLabel;
    QLineEdit *m_labelEdit;
    QLineEdit *m_artistEdit;
    QLineEdit *m_titleEdit;
    QLineEdit *m_albumEdit;
    QLineEdit *m_genreEdit;
    QSpinBox *m_yearSpin;
    // QSpinBox *m_trackSpin;
    QPlainTextEdit *m_commentEdit;

    QWidget *m_extraMetadataContainer;
    QToolButton *m_extraToggleBtn;

    QLabel *m_coverPreview;
    QPushButton *m_chooseCoverBtn;
    QPushButton *m_removeCoverBtn;
    QLabel *m_coverWarningLabel;

    // QCheckBox *m_rememberCheck;
    QMap<QString, QToolButton *> m_lockButtons;
    QWidget *makeLockableRow(QWidget *editor, const QString &settingsKey);
    void applyLockedValues();
    void persistLockedValues(const TrackMetadata &m);

    QPushButton *m_saveBtn;
    QPushButton *m_saveCopyBtn;
    QPushButton *m_anotherFileBtn;

    QLabel *m_statusLabel;
};
