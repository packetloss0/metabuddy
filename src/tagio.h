#pragma once
#include <QString>
#include <QByteArray>

struct TrackMetadata
{
    QString label;
    QString artist;
    QString title;
    QString album;
    QString genre;
    QString comment;
    int year = 0;
    // int track = 0;

    QByteArray coverData;
    QString coverMime;
    bool hasCover = false;
};

namespace TagIO
{

    enum class CoverSupport {
        FULL,
        LIMITED,
        NONE
    };

    CoverSupport coverSupport(const QString &path);

    bool isSupported(const QString &path);
    bool readMetadata(const QString &path, TrackMetadata &out);
    bool writeMetadata(const QString &path, const TrackMetadata &meta);

}
