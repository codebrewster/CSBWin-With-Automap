// AutoMapZip.cpp : see AutoMapZip.h
#include "AutoMapZip.h"

#include <cstring>

#include "miniz/miniz.h"

// Every zip archive starts with a local file header
static const char ZIP_MARKER[4] = {'P', 'K', 3, 4};

std::string AutoMapZip_Pack(const std::vector<AutoMapZipEntry> &entries)
{
  size_t totalSize = 0;
  for (size_t i = 0; i < entries.size(); i++) totalSize += entries[i].bytes.size();
  mz_zip_archive zip;
  std::memset(&zip, 0, sizeof(zip));
  if (!mz_zip_writer_init_heap(&zip, 0, totalSize / 4 + 1024)) return std::string();
  bool ok = true;
  for (size_t i = 0; i < entries.size() && ok; i++)
  {
    const AutoMapZipEntry &entry = entries[i];
    ok = mz_zip_writer_add_mem(&zip, entry.name.c_str(), entry.bytes.data(), entry.bytes.size(),
                               MZ_BEST_COMPRESSION) != MZ_FALSE;
  }
  void *archive = NULL;
  size_t archiveSize = 0;
  ok = ok && mz_zip_writer_finalize_heap_archive(&zip, &archive, &archiveSize);
  std::string packed;
  if (ok) packed.assign((const char *)archive, archiveSize);
  // A finalized archive belongs to the caller; one left unfinished is freed by End
  if (archive != NULL) mz_free(archive);
  mz_zip_writer_end(&zip);
  return packed;
}

AutoMapZipUnpackResult AutoMapZip_Unpack(const std::string &file, std::vector<AutoMapZipEntry> *entries)
{
  entries->clear();
  if (file.size() < sizeof(ZIP_MARKER) || std::memcmp(file.data(), ZIP_MARKER, sizeof(ZIP_MARKER)) != 0)
  {
    AutoMapZipEntry bare = {std::string(), file, true};
    entries->push_back(bare);
    return AutoMapZipWasBare;
  }
  mz_zip_archive zip;
  std::memset(&zip, 0, sizeof(zip));
  if (!mz_zip_reader_init_mem(&zip, file.data(), file.size(), 0)) return AutoMapZipIsBroken;
  mz_uint count = mz_zip_reader_get_num_files(&zip);
  for (mz_uint i = 0; i < count; i++)
  {
    AutoMapZipEntry entry = {std::string(), std::string(), false};
    char name[512];
    if (mz_zip_reader_get_filename(&zip, i, name, sizeof(name)) > 0) entry.name = name;
    size_t size = 0;
    void *data = mz_zip_reader_extract_to_heap(&zip, i, &size, 0);
    if (data != NULL)
    {
      entry.bytes.assign((const char *)data, size);
      entry.isReadable = true;
      mz_free(data);
    }
    entries->push_back(entry);
  }
  mz_zip_reader_end(&zip);
  if (entries->empty() || !(*entries)[0].isReadable)
  {
    entries->clear();
    return AutoMapZipIsBroken;
  }
  return AutoMapZipUnpacked;
}

const AutoMapZipEntry *AutoMapZip_Find(const std::vector<AutoMapZipEntry> &entries, const std::string &name)
{
  for (size_t i = 0; i < entries.size(); i++)
  {
    if (entries[i].name == name) return &entries[i];
  }
  return NULL;
}
