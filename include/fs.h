#pragma once

namespace FS {
    Result OpenArchive(FS_Archive *archive, FS_ArchiveID archiveID);
    Result CloseArchive(FS_Archive archive);
}