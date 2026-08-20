// Copyright 2023 Kushview, LLC <info@kushview.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "ElementApp.h"
#include <element/session.hpp>

namespace element {
class SessionDocument : public FileBasedDocument,
                        public ChangeListener
{
public:
    SessionDocument (SessionPtr);
    ~SessionDocument();

    String getDocumentTitle() override;
    Result loadDocument (const File& file) override;
    Result saveDocument (const File& file) override;
    File getLastDocumentOpened() override;
    void setLastDocumentOpened (const File& file) override;
    File getSuggestedSaveAsFile (const File& defaultFile) override
    {
        if (getFile().existsAsFile())
            return getFile().getNonexistentSibling (true);

        auto directory = preferredSaveAsDirectory;
        if (! directory.isDirectory())
            directory.createDirectory();

        return directory.isDirectory() ? directory.getChildFile (defaultFile.getFileName())
                                        : defaultFile;
    }

    void setPreferredSaveAsDirectory (const File& directory)
    {
        preferredSaveAsDirectory = directory;
    }

    void changeListenerCallback (ChangeBroadcaster*) override;

private:
    SessionPtr session;
    File lastSession;
    File preferredSaveAsDirectory;
    friend class Session;
    void onSessionChanged();
};
} // namespace element
