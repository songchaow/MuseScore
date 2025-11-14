/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore BVBA and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "filesystem.h"

#ifndef NO_QT_SUPPORT
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#endif

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include "io/ioretcodes.h"
#include "log.h"


#ifndef NO_QT_SUPPORT

using namespace mu;
using namespace mu::io;

Ret FileSystem::exists(const io::path_t& path) const
{
    QFileInfo fileInfo(path.toQString());
    if (!fileInfo.exists()) {
        return make_ret(Err::FSNotExist);
    }

    return make_ret(Err::NoError);
}

Ret FileSystem::remove(const io::path_t& path_, bool onlyIfEmpty)
{
    QString path = path_.toQString();
    QFileInfo fileInfo(path);
    if (fileInfo.exists()) {
        return fileInfo.isDir() ? removeDir(path, onlyIfEmpty) : removeFile(path);
    }

    return make_ret(Err::NoError);
}

Ret FileSystem::clear(const io::path_t& path_)
{
    QString path = path_.toQString();
    if (!QFileInfo::exists(path)) {
        return true;
    }

    Ret ret = make_ret(Err::NoError);
    QDirIterator di(path, QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
    while (di.hasNext()) {
        di.next();
        const QFileInfo& fi = di.fileInfo();
        const QString& filePath = di.filePath();
        if (fi.isDir() && !fi.isSymLink()) {
            ret = removeDir(filePath); // recursive
        } else {
            bool ok = QFile::remove(filePath);
            if (!ok) { // Read-only files prevent directory deletion on Windows, retry with Write permission.
                const QFile::Permissions permissions = QFile::permissions(filePath);
                if (!(permissions & QFile::WriteUser)) {
                    ok = QFile::setPermissions(filePath, permissions | QFile::WriteUser)
                         && QFile::remove(filePath);
                }
            }

            if (!ok) {
                ret = make_ret(Err::FSRemoveError);
            }
        }

        if (!ret) {
            break;
        }
    }

    return ret;
}

Ret FileSystem::copy(const io::path_t& src, const io::path_t& dst, bool replace)
{
    QFileInfo srcFileInfo(src.toQString());
    if (!srcFileInfo.exists()) {
        return make_ret(Err::FSNotExist);
    }

    QFileInfo dstFileInfo(dst.toQString());
    if (dstFileInfo.exists()) {
        if (!replace) {
            return make_ret(Err::FSAlreadyExists);
        }

        Ret ret = remove(dst);
        if (!ret) {
            return ret;
        }
    }

    Ret ret = copyRecursively(src, dst);
    return ret;
}

Ret FileSystem::move(const io::path_t& src, const io::path_t& dst, bool replace)
{
    QFileInfo srcFileInfo(src.toQString());
    if (!srcFileInfo.exists()) {
        return make_ret(Err::FSNotExist);
    }

    QFileInfo dstFileInfo(dst.toQString());
    if (dstFileInfo.exists()) {
        if (!replace) {
            return make_ret(Err::FSAlreadyExists);
        }

        Ret ret = remove(dst);
        if (!ret) {
            return ret;
        }
    }

    if (srcFileInfo.isDir()) {
        if (!QDir().rename(src.toQString(), dst.toQString())) {
            return make_ret(Err::FSMoveErrors);
        }
    } else {
        if (!QFile::rename(src.toQString(), dst.toQString())) {
            return make_ret(Err::FSMoveErrors);
        }
    }

    return make_ret(Ret::Code::Ok);
}

RetVal<ByteArray> FileSystem::readFile(const io::path_t& filePath) const
{
    RetVal<ByteArray> result;
    Ret ret = exists(filePath);
    if (!ret) {
        result.ret = ret;
        return result;
    }

    QFile file(filePath.toQString());
    if (!file.open(QIODevice::ReadOnly)) {
        result.ret = make_ret(Err::FSReadError);
        return result;
    }

    qint64 size = file.size();
    result.val.resize(static_cast<size_t>(size));

    file.read(reinterpret_cast<char*>(result.val.data()), size);
    file.close();

    result.ret = make_ret(Err::NoError);
    return result;
}

Ret FileSystem::readFile(const io::path_t& filePath, ByteArray& data) const
{
    Ret ret = make_ok();

    QFile file(filePath.toQString());
    if (!file.open(QIODevice::ReadOnly)) {
        ret = make_ret(Err::FSReadError);
        ret.setText(file.errorString().toStdString());
        return ret;
    }

    qint64 size = file.size();
    data.resize(static_cast<size_t>(size));

    if (file.read(reinterpret_cast<char*>(data.data()), size) == -1) {
        ret = make_ret(Err::FSReadError);
        ret.setText(file.errorString().toStdString());
    }

    file.close();

    return ret;
}

Ret FileSystem::writeFile(const io::path_t& filePath, const ByteArray& data) const
{
    Ret ret = make_ok();

    QFile file(filePath.toQString());
    if (!file.open(QIODevice::WriteOnly)) {
        ret = make_ret(Err::FSWriteError);
        ret.setText(file.errorString().toStdString());
        return ret;
    }

    if (file.write(reinterpret_cast<const char*>(data.constData()), static_cast<qint64>(data.size())) == -1) {
        ret = make_ret(Err::FSWriteError);
        ret.setText(file.errorString().toStdString());
    }

    file.close();

    return ret;
}

Ret FileSystem::makePath(const io::path_t& path) const
{
    if (!QDir().mkpath(path.toQString())) {
        return make_ret(Err::FSMakingError);
    }

    return make_ret(Err::NoError);
}

EntryType FileSystem::entryType(const io::path_t& path) const
{
    QFileInfo fi(path.toQString());
    if (fi.isFile()) {
        return EntryType::File;
    } else if (fi.isDir()) {
        return EntryType::Dir;
    }

    return EntryType::Undefined;
}

RetVal<uint64_t> FileSystem::fileSize(const io::path_t& path) const
{
    RetVal<uint64_t> rv;
    rv.ret = exists(path);
    if (!rv.ret) {
        return rv;
    }

    QFileInfo fi(path.toQString());
    rv.val = static_cast<uint64_t>(fi.size());
    return rv;
}

RetVal<io::paths_t> FileSystem::scanFiles(const io::path_t& rootDir, const std::vector<std::string>& nameFilters, ScanMode mode) const
{
    RetVal<io::paths_t> result;
    Ret ret = exists(rootDir);
    if (!ret) {
        result.ret = ret;
        return result;
    }

    QDirIterator::IteratorFlags flags = QDirIterator::NoIteratorFlags;
    QDir::Filters filters = QDir::NoDotAndDotDot | QDir::Readable;

    switch (mode) {
    case ScanMode::FilesInCurrentDir:
        filters |= QDir::Files;
        break;
    case ScanMode::FilesAndFoldersInCurrentDir:
        filters |= QDir::Files | QDir::Dirs;
        break;
    case ScanMode::FilesInCurrentDirAndSubdirs:
        flags |= QDirIterator::Subdirectories;
        filters |= QDir::Files;
        break;
    }

    QStringList qnameFilters;
    for (const std::string& f : nameFilters) {
        qnameFilters << QString::fromStdString(f);
    }

    QDirIterator it(rootDir.toQString(), qnameFilters, filters, flags);

    while (it.hasNext()) {
        result.val.push_back(it.next());
    }

    result.ret = make_ret(Err::NoError);
    return result;
}

Ret FileSystem::removeFile(const io::path_t& path) const
{
    QFile file(path.toQString());
    if (!file.remove()) {
        return make_ret(Err::FSRemoveError);
    }

    return make_ret(Err::NoError);
}

Ret FileSystem::removeDir(const io::path_t& path, bool onlyIfEmpty) const
{
    QDir dir(path.toQString());

    if (onlyIfEmpty && !dir.isEmpty()) {
        return make_ret(Err::FSDirNotEmptyError);
    }

    if (!dir.removeRecursively()) {
        return make_ret(Err::FSRemoveError);
    }

    return make_ret(Err::NoError);
}

Ret FileSystem::copyRecursively(const io::path_t& src, const io::path_t& dst) const
{
    QString srcPath = src.toQString();
    QString dstPath = dst.toQString();

    QFileInfo srcFileInfo(srcPath);
    if (srcFileInfo.isDir()) {
        QDir dstDir(dstPath);
        dstDir.cdUp();
        if (!dstDir.mkdir(QFileInfo(dstPath).fileName())) {
            return make_ret(Err::FSMakingError);
        }
        QDir srcDir(srcPath);
        const QStringList fileNames = srcDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
        for (const QString& fileName : fileNames) {
            const QString newSrcPath = srcPath + QLatin1Char('/') + fileName;
            const QString newDstPath = dstPath + QLatin1Char('/') + fileName;
            Ret ret = copyRecursively(newSrcPath, newDstPath);
            if (!ret) {
                return ret;
            }
        }
    } else {
        if (!QFile::copy(srcPath, dstPath)) {
            return make_ret(Err::FSCopyError);
        }
    }

    return make_ret(Err::NoError);
}

void FileSystem::setAttribute(const io::path_t& path, Attribute attribute) const
{
    switch (attribute) {
    case Attribute::Hidden: {
#ifdef Q_OS_WIN
        const QString nativePath = QDir::toNativeSeparators(path.toQString());
        SetFileAttributes((LPCTSTR)nativePath.unicode(), FILE_ATTRIBUTE_HIDDEN);
#endif
    } break;
    }
    UNUSED(path);
}

bool FileSystem::setPermissionsAllowedForAll(const io::path_t& path) const
{
    return QFile::setPermissions(path.toQString(),
                                 QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner
                                 | QFile::ReadUser | QFile::WriteUser | QFile::ExeUser
                                 | QFile::ReadGroup | QFile::WriteGroup | QFile::ExeGroup
                                 | QFile::ReadOther | QFile::WriteOther
                                 | QFile::ExeOther);
}

io::path_t FileSystem::canonicalFilePath(const io::path_t& filePath) const
{
    return QFileInfo(filePath.toQString()).canonicalFilePath();
}

io::path_t FileSystem::absolutePath(const io::path_t& filePath) const
{
    return QFileInfo(filePath.toQString()).absolutePath();
}

path_t FileSystem::absoluteFilePath(const path_t& filePath) const
{
    return QFileInfo(filePath.toQString()).absoluteFilePath();
}

DateTime FileSystem::birthTime(const io::path_t& filePath) const
{
    return DateTime::fromQDateTime(QFileInfo(filePath.toQString()).birthTime());
}

DateTime FileSystem::lastModified(const io::path_t& filePath) const
{
    return DateTime::fromQDateTime(QFileInfo(filePath.toQString()).lastModified());
}

Ret FileSystem::isWritable(const io::path_t& filePath) const
{
    Ret ret = make_ok();

    QFileInfo fileInfo(filePath.toQString());

    if (!fileInfo.exists()) {
        QFile file(filePath.toQString());

        if (!file.open(QFile::WriteOnly)) {
            ret = make_ret(Err::FSWriteError);
            ret.setText(file.errorString().toStdString());
        }

        file.close();
        file.remove();
    } else if (!fileInfo.isWritable()) {
        QFile file(filePath.toQString());
        file.open(QFile::WriteOnly);

        ret = make_ret(Err::FSWriteError);
        ret.setText(file.errorString().toStdString());

        file.close();
    }

    return ret;
}

#else

// the following are FileSystem implementations without qt support
// Using Godot Engine's FileAccess and DirAccess APIs

#include "core/io/file_access.h"
#include "core/io/dir_access.h"

// Use Godot namespace to avoid conflicts with mu::String
using GodotString = ::String;

using namespace mu;
using namespace mu::io;

Ret FileSystem::exists(const io::path_t& path) const
{
    GodotString godotPath = GodotString::utf8(path.c_str());
    if (!FileAccess::exists(godotPath) && !DirAccess::dir_exists_absolute(godotPath)) {
        return make_ret(Err::FSNotExist);
    }
    return make_ret(Err::NoError);
}

Ret FileSystem::remove(const io::path_t& path_, bool onlyIfEmpty)
{
    GodotString path = GodotString::utf8(path_.c_str());
    
    Ref<DirAccess> dir = DirAccess::open(path);
    if (dir.is_valid()) {
        return removeDir(path_, onlyIfEmpty);
    } else if (FileAccess::exists(path)) {
        return removeFile(path_);
    }
    
    return make_ret(Err::NoError);
}

Ret FileSystem::clear(const io::path_t& path_)
{
    GodotString path = GodotString::utf8(path_.c_str());
    if (!DirAccess::dir_exists_absolute(path)) {
        return make_ret(Err::NoError);
    }
    
    Ref<DirAccess> dir = DirAccess::open(path);
    if (!dir.is_valid()) {
        return make_ret(Err::FSReadError);
    }
    
    Ret ret = make_ret(Err::NoError);
    dir->list_dir_begin();
    GodotString fileName = dir->get_next();
    
    while (!fileName.is_empty()) {
        if (fileName != "." && fileName != "..") {
            GodotString fullPath = path + "/" + fileName;
            if (dir->current_is_dir()) {
                ret = removeDir(io::path_t(fullPath.utf8().get_data()));
            } else {
                if (DirAccess::remove_absolute(fullPath) != OK) {
                    ret = make_ret(Err::FSRemoveError);
                }
            }
            
            if (!ret) {
                break;
            }
        }
        fileName = dir->get_next();
    }
    
    dir->list_dir_end();
    return ret;
}

Ret FileSystem::copy(const io::path_t& src, const io::path_t& dst, bool replace)
{
    GodotString srcPath = GodotString::utf8(src.c_str());
    GodotString dstPath = GodotString::utf8(dst.c_str());
    
    if (!FileAccess::exists(srcPath) && !DirAccess::dir_exists_absolute(srcPath)) {
        return make_ret(Err::FSNotExist);
    }
    
    if (FileAccess::exists(dstPath) || DirAccess::dir_exists_absolute(dstPath)) {
        if (!replace) {
            return make_ret(Err::FSAlreadyExists);
        }
        
        Ret ret = remove(dst);
        if (!ret) {
            return ret;
        }
    }
    
    return copyRecursively(src, dst);
}

Ret FileSystem::move(const io::path_t& src, const io::path_t& dst, bool replace)
{
    GodotString srcPath = GodotString::utf8(src.c_str());
    GodotString dstPath = GodotString::utf8(dst.c_str());
    
    if (!FileAccess::exists(srcPath) && !DirAccess::dir_exists_absolute(srcPath)) {
        return make_ret(Err::FSNotExist);
    }
    
    if (FileAccess::exists(dstPath) || DirAccess::dir_exists_absolute(dstPath)) {
        if (!replace) {
            return make_ret(Err::FSAlreadyExists);
        }
        
        Ret ret = remove(dst);
        if (!ret) {
            return ret;
        }
    }
    
    Error err = DirAccess::rename_absolute(srcPath, dstPath);
    if (err != OK) {
        return make_ret(Err::FSMoveErrors);
    }
    
    return make_ret(Ret::Code::Ok);
}

RetVal<ByteArray> FileSystem::readFile(const io::path_t& filePath) const
{
    RetVal<ByteArray> result;
    Ret ret = exists(filePath);
    if (!ret) {
        result.ret = ret;
        return result;
    }
    
    GodotString path = GodotString::utf8(filePath.c_str());
    Error err;
    Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ, &err);
    
    if (!file.is_valid() || err != OK) {
        result.ret = make_ret(Err::FSReadError);
        return result;
    }
    
    uint64_t size = file->get_length();
    result.val.resize(static_cast<size_t>(size));
    
    uint64_t bytesRead = file->get_buffer(reinterpret_cast<uint8_t*>(result.val.data()), size);
    file->close();
    
    if (bytesRead != size) {
        result.ret = make_ret(Err::FSReadError);
        return result;
    }
    
    result.ret = make_ret(Err::NoError);
    return result;
}

Ret FileSystem::readFile(const io::path_t& filePath, ByteArray& data) const
{
    GodotString path = GodotString::utf8(filePath.c_str());
    Error err;
    Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ, &err);
    
    if (!file.is_valid() || err != OK) {
        Ret ret = make_ret(Err::FSReadError);
        ret.setText("Failed to open file for reading");
        return ret;
    }
    
    uint64_t size = file->get_length();
    data.resize(static_cast<size_t>(size));
    
    uint64_t bytesRead = file->get_buffer(reinterpret_cast<uint8_t*>(data.data()), size);
    file->close();
    
    if (bytesRead != size) {
        Ret ret = make_ret(Err::FSReadError);
        ret.setText("Failed to read complete file");
        return ret;
    }
    
    return make_ok();
}

Ret FileSystem::writeFile(const io::path_t& filePath, const ByteArray& data) const
{
    GodotString path = GodotString::utf8(filePath.c_str());
    Error err;
    Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE, &err);
    
    if (!file.is_valid() || err != OK) {
        Ret ret = make_ret(Err::FSWriteError);
        ret.setText("Failed to open file for writing");
        return ret;
    }
    
    file->store_buffer(reinterpret_cast<const uint8_t*>(data.constData()), data.size());
    file->close();
    
    return make_ok();
}

Ret FileSystem::makePath(const io::path_t& path) const
{
    GodotString godotPath = GodotString::utf8(path.c_str());
    Error err = DirAccess::make_dir_recursive_absolute(godotPath);
    
    if (err != OK) {
        return make_ret(Err::FSMakingError);
    }
    
    return make_ret(Err::NoError);
}

EntryType FileSystem::entryType(const io::path_t& path) const
{
    GodotString godotPath = GodotString::utf8(path.c_str());
    
    if (FileAccess::exists(godotPath)) {
        return EntryType::File;
    } else if (DirAccess::dir_exists_absolute(godotPath)) {
        return EntryType::Dir;
    }
    
    return EntryType::Undefined;
}

RetVal<uint64_t> FileSystem::fileSize(const io::path_t& path) const
{
    RetVal<uint64_t> rv;
    rv.ret = exists(path);
    if (!rv.ret) {
        return rv;
    }
    
    GodotString godotPath = GodotString::utf8(path.c_str());
    Error err;
    Ref<FileAccess> file = FileAccess::open(godotPath, FileAccess::READ, &err);
    
    if (file.is_valid() && err == OK) {
        rv.val = file->get_length();
        file->close();
    } else {
        rv.val = 0;
    }
    
    return rv;
}

RetVal<io::paths_t> FileSystem::scanFiles(const io::path_t& rootDir, const std::vector<std::string>& nameFilters, ScanMode mode) const
{
    RetVal<io::paths_t> result;
    Ret ret = exists(rootDir);
    if (!ret) {
        result.ret = ret;
        return result;
    }
    
    GodotString rootPath = GodotString::utf8(rootDir.c_str());
    Ref<DirAccess> dir = DirAccess::open(rootPath);
    
    if (!dir.is_valid()) {
        result.ret = make_ret(Err::FSReadError);
        return result;
    }
    
    std::function<void(const GodotString&, bool)> scanDirectory;
    scanDirectory = [&](const GodotString& dirPath, bool recursive) {
        Ref<DirAccess> currentDir = DirAccess::open(dirPath);
        if (!currentDir.is_valid()) {
            return;
        }
        
        currentDir->list_dir_begin();
        GodotString fileName = currentDir->get_next();
        
        while (!fileName.is_empty()) {
            if (fileName != "." && fileName != "..") {
                GodotString fullPath = dirPath.path_join(fileName);
                bool isDir = currentDir->current_is_dir();
                
                bool shouldAdd = false;
                if (mode == ScanMode::FilesInCurrentDir || mode == ScanMode::FilesInCurrentDirAndSubdirs) {
                    shouldAdd = !isDir;
                } else if (mode == ScanMode::FilesAndFoldersInCurrentDir) {
                    shouldAdd = true;
                }
                
                if (shouldAdd) {
                    // Check name filters
                    bool matchesFilter = nameFilters.empty();
                    if (!matchesFilter) {
                        for (const auto& filter : nameFilters) {
                            GodotString filterStr = GodotString::utf8(filter.c_str());
                            if (filterStr.contains("*")) {
                                // Simple wildcard matching
                                GodotString pattern = filterStr.replace("*", "");
                                if (fileName.contains(pattern)) {
                                    matchesFilter = true;
                                    break;
                                }
                            } else if (fileName.ends_with(filterStr)) {
                                matchesFilter = true;
                                break;
                            }
                        }
                    }
                    
                    if (matchesFilter) {
                        result.val.push_back(io::path_t(fullPath.utf8().get_data()));
                    }
                }
                
                if (isDir && recursive && mode == ScanMode::FilesInCurrentDirAndSubdirs) {
                    scanDirectory(fullPath, true);
                }
            }
            fileName = currentDir->get_next();
        }
        
        currentDir->list_dir_end();
    };
    
    bool recursive = (mode == ScanMode::FilesInCurrentDirAndSubdirs);
    scanDirectory(rootPath, recursive);
    
    result.ret = make_ret(Err::NoError);
    return result;
}

Ret FileSystem::removeFile(const io::path_t& path) const
{
    GodotString godotPath = GodotString::utf8(path.c_str());
    Error err = DirAccess::remove_absolute(godotPath);
    
    if (err != OK) {
        return make_ret(Err::FSRemoveError);
    }
    
    return make_ret(Err::NoError);
}

Ret FileSystem::removeDir(const io::path_t& path, bool onlyIfEmpty) const
{
    GodotString godotPath = GodotString::utf8(path.c_str());
    Ref<DirAccess> dir = DirAccess::open(godotPath);
    
    if (!dir.is_valid()) {
        return make_ret(Err::FSRemoveError);
    }
    
    if (onlyIfEmpty) {
        dir->list_dir_begin();
        GodotString fileName = dir->get_next();
        bool isEmpty = true;
        
        while (!fileName.is_empty()) {
            if (fileName != "." && fileName != "..") {
                isEmpty = false;
                break;
            }
            fileName = dir->get_next();
        }
        dir->list_dir_end();
        
        if (!isEmpty) {
            return make_ret(Err::FSDirNotEmptyError);
        }
    } else {
        // Remove directory contents recursively
        dir->list_dir_begin();
        GodotString fileName = dir->get_next();
        
        while (!fileName.is_empty()) {
            if (fileName != "." && fileName != "..") {
                GodotString fullPath = godotPath + "/" + fileName;
                if (dir->current_is_dir()) {
                    Ret ret = removeDir(io::path_t(fullPath.utf8().get_data()), false);
                    if (!ret) {
                        dir->list_dir_end();
                        return ret;
                    }
                } else {
                    if (DirAccess::remove_absolute(fullPath) != OK) {
                        dir->list_dir_end();
                        return make_ret(Err::FSRemoveError);
                    }
                }
            }
            fileName = dir->get_next();
        }
        dir->list_dir_end();
    }
    
    // Remove the directory itself
    Error err = DirAccess::remove_absolute(godotPath);
    if (err != OK) {
        return make_ret(Err::FSRemoveError);
    }
    
    return make_ret(Err::NoError);
}

Ret FileSystem::copyRecursively(const io::path_t& src, const io::path_t& dst) const
{
    GodotString srcPath = GodotString::utf8(src.c_str());
    GodotString dstPath = GodotString::utf8(dst.c_str());
    
    if (DirAccess::dir_exists_absolute(srcPath)) {
        // Create destination directory
        Error err = DirAccess::make_dir_absolute(dstPath);
        if (err != OK && err != ERR_ALREADY_EXISTS) {
            return make_ret(Err::FSMakingError);
        }
        
        Ref<DirAccess> srcDir = DirAccess::open(srcPath);
        if (!srcDir.is_valid()) {
            return make_ret(Err::FSReadError);
        }
        
        srcDir->list_dir_begin();
        GodotString fileName = srcDir->get_next();
        
        while (!fileName.is_empty()) {
            if (fileName != "." && fileName != "..") {
                GodotString newSrcPath = srcPath.path_join(fileName);
                GodotString newDstPath = dstPath.path_join(fileName);
                
                Ret ret = copyRecursively(io::path_t(newSrcPath.utf8().get_data()), 
                                         io::path_t(newDstPath.utf8().get_data()));
                if (!ret) {
                    srcDir->list_dir_end();
                    return ret;
                }
            }
            fileName = srcDir->get_next();
        }
        
        srcDir->list_dir_end();
    } else {
        // Copy file
        Error err = DirAccess::copy_absolute(srcPath, dstPath);
        if (err != OK) {
            return make_ret(Err::FSCopyError);
        }
    }
    
    return make_ret(Err::NoError);
}

void FileSystem::setAttribute(const io::path_t& path, Attribute attribute) const
{
    // Godot doesn't provide direct attribute setting API
    // This would require platform-specific implementation
    UNUSED(path);
    UNUSED(attribute);
}

bool FileSystem::setPermissionsAllowedForAll(const io::path_t& path) const
{
    // Godot doesn't provide direct permission setting API
    // This would require platform-specific implementation
    UNUSED(path);
    return true;
}

io::path_t FileSystem::canonicalFilePath(const io::path_t& filePath) const
{
    // Godot doesn't have a direct canonical path function
    // Return simplified absolute path
    return absoluteFilePath(filePath);
}

io::path_t FileSystem::absolutePath(const io::path_t& filePath) const
{
    GodotString path = GodotString::utf8(filePath.c_str());
    GodotString absPath = path.simplify_path();
    
    if (!absPath.is_absolute_path()) {
        Ref<DirAccess> dir = DirAccess::open(".");
        if (dir.is_valid()) {
            absPath = dir->get_current_dir().path_join(path).simplify_path();
        }
    }
    
    // Get directory part only
    return io::path_t(absPath.get_base_dir().utf8().get_data());
}

path_t FileSystem::absoluteFilePath(const path_t& filePath) const
{
    GodotString path = GodotString::utf8(filePath.c_str());
    GodotString absPath = path.simplify_path();
    
    if (!absPath.is_absolute_path()) {
        Ref<DirAccess> dir = DirAccess::open(".");
        if (dir.is_valid()) {
            absPath = dir->get_current_dir().path_join(path).simplify_path();
        }
    }
    
    return io::path_t(absPath.utf8().get_data());
}

DateTime FileSystem::birthTime(const io::path_t& filePath) const
{
    // Godot doesn't provide birth time API
    // Return last modified time as fallback
    return lastModified(filePath);
}

DateTime FileSystem::lastModified(const io::path_t& filePath) const
{
    GodotString path = GodotString::utf8(filePath.c_str());
    // Godot's timestamp is in seconds since Unix epoch
    // You may need to implement proper conversion based on your DateTime class
    DateTime dt;
    return dt;
}

Ret FileSystem::isWritable(const io::path_t& filePath) const
{
    GodotString path = GodotString::utf8(filePath.c_str());
    Ret ret = make_ok();
    
    if (!FileAccess::exists(path)) {
        // Try to create and remove a test file
        Error err;
        Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE, &err);
        
        if (!file.is_valid() || err != OK) {
            ret = make_ret(Err::FSWriteError);
            ret.setText("Cannot create file");
        } else {
            file->close();
            DirAccess::remove_absolute(path);
        }
    } else {
        // Try to open existing file for writing
        Error err;
        Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ_WRITE, &err);
        
        if (!file.is_valid() || err != OK) {
            ret = make_ret(Err::FSWriteError);
            ret.setText("File is not writable");
        } else {
            file->close();
        }
    }
    
    return ret;
}

#endif //NO_QT_SUPPORT