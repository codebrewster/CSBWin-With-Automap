// AutoMapZip.cpp : see AutoMapZip.h
#include "AutoMapZip.h"

#include <cstring>

#include "miniz/miniz.h"

// Every zip archive starts with a local file header
static const char ZIP_MARKER[4] = {'P', 'K', 3, 4};

std::string AutoMapZip_Pack(const std::string &entryName, const std::string &bytes)
{
  mz_zip_archive zip;
  std::memset(&zip, 0, sizeof(zip));
  if (!mz_zip_writer_init_heap(&zip, 0, bytes.size() / 4 + 1024)) return std::string();
  void *archive = NULL;
  size_t archiveSize = 0;
  bool ok = mz_zip_writer_add_mem(&zip, entryName.c_str(), bytes.data(), bytes.size(), MZ_BEST_COMPRESSION) &&
            mz_zip_writer_finalize_heap_archive(&zip, &archive, &archiveSize);
  std::string packed;
  if (ok) packed.assign((const char *)archive, archiveSize);
  // A finalized archive belongs to the caller; one left unfinished is freed by End
  if (archive != NULL) mz_free(archive);
  mz_zip_writer_end(&zip);
  return packed;
}

AutoMapZipUnpackResult AutoMapZip_Unpack(const std::string &file, std::string *bytes)
{
  if (file.size() < sizeof(ZIP_MARKER) || std::memcmp(file.data(), ZIP_MARKER, sizeof(ZIP_MARKER)) != 0)
  {
    *bytes = file;
    return AutoMapZipWasBare;
  }
  mz_zip_archive zip;
  std::memset(&zip, 0, sizeof(zip));
  if (!mz_zip_reader_init_mem(&zip, file.data(), file.size(), 0)) return AutoMapZipIsBroken;
  AutoMapZipUnpackResult result = AutoMapZipIsBroken;
  size_t size = 0;
  void *entry = mz_zip_reader_get_num_files(&zip) > 0 ? mz_zip_reader_extract_to_heap(&zip, 0, &size, 0) : NULL;
  if (entry != NULL)
  {
    bytes->assign((const char *)entry, size);
    mz_free(entry);
    result = AutoMapZipUnpacked;
  }
  mz_zip_reader_end(&zip);
  return result;
}
