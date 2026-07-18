#include "tagio.h"

#include <QFileInfo>

#include <fileref.h>
#include <tag.h>
#include <tstring.h>
#include <tbytevector.h>
#include <tpropertymap.h>

#include <mpegfile.h>
#include <id3v2tag.h>
#include <attachedpictureframe.h>

#include <flacfile.h>
#include <flacpicture.h>

#include <aifffile.h>
#include <wavfile.h>

namespace {

QString toQString(const TagLib::String &s) {
    return QString::fromUtf8(s.toCString(true));
}

TagLib::String toTagString(const QString &s) {
    return TagLib::String(s.toUtf8().constData(), TagLib::String::UTF8);
}

// Pulls the first APIC picture out of an ID3v2 tag (used for MP3 and AIFF).
void readId3v2Cover(TagLib::ID3v2::Tag *id3, TrackMetadata &out) {
    if (!id3) return;
    const auto frames = id3->frameList("APIC");
    if (frames.isEmpty()) return;
    auto *pic = static_cast<TagLib::ID3v2::AttachedPictureFrame *>(frames.front());
    const TagLib::ByteVector &data = pic->picture();
    out.coverData = QByteArray(data.data(), static_cast<int>(data.size()));
    out.coverMime = toQString(pic->mimeType());
    out.hasCover = true;
}

// Removes existing APIC frames and (optionally) adds a new one.
void writeId3v2Cover(TagLib::ID3v2::Tag *id3, const TrackMetadata &meta) {
    if (!id3) return;
    const auto frames = id3->frameList("APIC");
    // Copy the list first: removeFrame mutates the underlying list.
    QList<TagLib::ID3v2::Frame *> toRemove;
    for (auto *f : frames) toRemove.append(f);
    for (auto *f : toRemove) id3->removeFrame(f, true);

    if (meta.hasCover && !meta.coverData.isEmpty()) {
        auto *pic = new TagLib::ID3v2::AttachedPictureFrame();
        pic->setMimeType(toTagString(meta.coverMime));
        pic->setType(TagLib::ID3v2::AttachedPictureFrame::FrontCover);
        pic->setPicture(TagLib::ByteVector(meta.coverData.constData(), meta.coverData.size()));
        id3->addFrame(pic);
    }
}

} // namespace

TagIO::CoverSupport TagIO::coverSupport(const QString &path){
    const QString ext = QFileInfo(path).suffix().toLower();
    if(ext == "mp3" || ext == "flac") return CoverSupport::FULL;
    if(ext == "wav" || ext == "aiff" || ext == "aif") return CoverSupport::LIMITED;
    return CoverSupport::NONE; // anything else ogg,m4a etc.
}

bool TagIO::isSupported(const QString &path) {
    static const QStringList exts = {"mp3", "flac", "aiff", "aif", "wav", "ogg", "m4a"};
    return exts.contains(QFileInfo(path).suffix().toLower());
}

bool TagIO::readMetadata(const QString &path, TrackMetadata &out) {
    const std::string p = path.toUtf8().constData();

    TagLib::FileRef f(p.c_str());
    if (f.isNull() || !f.tag()) return false;

    TagLib::Tag *tag = f.tag();
    out.artist = toQString(tag->artist());
    out.title = toQString(tag->title());
    out.album = toQString(tag->album());
    out.genre = toQString(tag->genre());
    out.comment = toQString(tag->comment());
    out.year = static_cast<int>(tag->year());
    // out.track = static_cast<int>(tag->track());
    out.hasCover = false;

    if (f.file()) {
        TagLib::PropertyMap props = f.file()->properties();
        if(props.contains("LABEL"))
            out.label = toQString(props["LABEL"].toString());
    }

    const QString ext = QFileInfo(path).suffix().toLower();

    if (ext == "mp3") {
        TagLib::MPEG::File mf(p.c_str());
        if (mf.isValid()) readId3v2Cover(mf.ID3v2Tag(), out);
    } else if (ext == "flac") {
        TagLib::FLAC::File ff(p.c_str());
        if (ff.isValid()) {
            const auto pics = ff.pictureList();
            if (!pics.isEmpty()) {
                auto *pic = pics.front();
                out.coverData = QByteArray(pic->data().data(), static_cast<int>(pic->data().size()));
                out.coverMime = toQString(pic->mimeType());
                out.hasCover = true;
            }
        }
    } else if (ext == "aiff" || ext == "aif") {
        TagLib::RIFF::AIFF::File af(p.c_str());
        if (af.isValid()) readId3v2Cover(af.tag(), out);
    } else if (ext == "wav") {
        TagLib::RIFF::WAV::File wf(p.c_str());
        if (wf.isValid()) readId3v2Cover(wf.ID3v2Tag(), out);
    }

    return true;
}

bool TagIO::writeMetadata(const QString &path, const TrackMetadata &meta) {
    const std::string p = path.toUtf8().constData();

    // 1. Write the plain text fields generically.
    {
        TagLib::FileRef f(p.c_str());
        if (f.isNull() || !f.tag()) return false;
        TagLib::Tag *tag = f.tag();
        tag->setArtist(toTagString(meta.artist));
        tag->setTitle(toTagString(meta.title));
        tag->setAlbum(toTagString(meta.album));
        tag->setGenre(toTagString(meta.genre));
        tag->setComment(toTagString(meta.comment));
        tag->setYear(meta.year > 0 ? static_cast<unsigned int>(meta.year) : 0);
        // tag->setTrack(meta.track > 0 ? static_cast<unsigned int>(meta.track) : 0);

        if(f.file()){
            TagLib::PropertyMap props = f.file()->properties();
            props.replace("LABEL", TagLib::StringList(toTagString(meta.label)));
            f.file()->setProperties(props);
        }

        if (!f.save()) return false;
    }

    // 2. Cover art needs format-specific handling.
    const QString ext = QFileInfo(path).suffix().toLower();

    if (ext == "mp3") {
        TagLib::MPEG::File mf(p.c_str());
        if (!mf.isValid()) return false;
        writeId3v2Cover(mf.ID3v2Tag(true), meta);
        return mf.save();
    } else if (ext == "flac") {
        TagLib::FLAC::File ff(p.c_str());
        if (!ff.isValid()) return false;
        ff.removePictures();
        if (meta.hasCover && !meta.coverData.isEmpty()) {
            auto *pic = new TagLib::FLAC::Picture();
            pic->setMimeType(toTagString(meta.coverMime));
            pic->setType(TagLib::FLAC::Picture::FrontCover);
            pic->setData(TagLib::ByteVector(meta.coverData.constData(), meta.coverData.size()));
            ff.addPicture(pic);
        }
        return ff.save();
    } else if (ext == "aiff" || ext == "aif") {
        TagLib::RIFF::AIFF::File af(p.c_str());
        if (!af.isValid()) return false;
        writeId3v2Cover(af.tag(), meta);
        return af.save();
    } else if (ext == "wav") {
        TagLib::RIFF::WAV::File wf(p.c_str());
        if (!wf.isValid()) return false;
        writeId3v2Cover(wf.ID3v2Tag(), meta);
        return wf.save();
    }

    return true;
}
