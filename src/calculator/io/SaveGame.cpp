#include "SaveGame.h"

#include <cstring>

#include "Adf.h"

namespace calc {

namespace {

const char kSentinel[] = " Extended Features ";   // 19 characters and their NUL
// (stream size, record size) of the hero data: with or without portraits
const size_t kHeroVariants[2][2] = {{1408, 320}, {3328, 800}};
// DB record sizes (CSB.h); DB11 holds 256-byte container records
const int kDbSize[16] = {4, 6, 4, 10, 16, 4, 4, 6, 4, 8, 4, 256, 2, 2, 8, 4};
const int kMaxItem16 = 2000, kMaxTimers = 3000;

uint16_t get16(const uint8_t *p, bool le)
{
  return le ? uint16_t(p[0] | (p[1] << 8)) : uint16_t((p[0] << 8) | p[1]);
}
void put16(uint8_t *p, bool le, uint16_t v)
{
  if (le) {
    p[0] = uint8_t(v);
    p[1] = uint8_t(v >> 8);
  } else {
    p[0] = uint8_t(v >> 8);
    p[1] = uint8_t(v);
  }
}

// Unscramble() (CSBCode.cpp:8999): decodes in place and gives the checksum
uint16_t unscramble(uint8_t *buf, uint16_t hash, size_t words, bool le)
{
  uint16_t d7 = hash, d6 = uint16_t(words), d5 = hash;
  for (size_t i = 0; i < words; i++) {
    uint16_t w = get16(buf + 2 * i, le);
    d5 = uint16_t(d5 + w);
    uint16_t w2 = w ^ d7;
    put16(buf + 2 * i, le, w2);
    d5 = uint16_t(d5 + w2);
    d7 = uint16_t(d7 + d6);
    d6--;
  }
  return d5;
}

// Block 1: its second half is scrambled with a key from its first half
bool decodeBlock1(const uint8_t *raw, bool le, Bytes &out)
{
  for (size_t keyAt : {size_t(20), size_t(58)}) {   // Dungeon Master, Chaos Strikes Back
    Bytes buf(raw, raw + 512);
    unscramble(buf.data() + 256, get16(buf.data() + keyAt, le), 128, le);
    uint16_t d6 = 0;
    for (size_t g = 0; g < 32; g++) {
      d6 = uint16_t(d6 + get16(buf.data() + 8 * g, le));
      d6 ^= get16(buf.data() + 8 * g + 2, le);
      d6 = uint16_t(d6 - get16(buf.data() + 8 * g + 4, le));
      d6 ^= get16(buf.data() + 8 * g + 6, le);
    }
    uint16_t d5 = 0;
    for (size_t i = 0; i < 128; i++) d5 = uint16_t(d5 + get16(buf.data() + 256 + 2 * i, le));
    if (d6 == d5) {
      out.swap(buf);
      return true;
    }
  }
  return false;
}

bool decodeStream(const Bytes &data, size_t at, size_t size, uint16_t hash, uint16_t checksum,
                  bool le, Bytes &out)
{
  if (at > data.size() || size > data.size() - at) return false;
  Bytes buf(data.begin() + long(at), data.begin() + long(at + size));
  if (unscramble(buf.data(), hash, size / 2, le) != checksum) return false;
  out.swap(buf);
  return true;
}

struct Located {
  size_t off = 0;         // where block 1 starts
  bool le = false;
  size_t hashAt = 0, checkAt = 0;
  Bytes block2;
  Bytes heroes;           // the decoded hero stream
  size_t heroRecord = 0;
  size_t heroAt = 0;      // file offset of the hero stream
  uint16_t hash[5] = {}, check[5] = {};
};

bool tryChain(const Bytes &data, size_t off, bool le, size_t hashAt, size_t checkAt, Located &loc)
{
  if (off > data.size() || data.size() - off < 640) return false;
  Bytes b1;
  if (!decodeBlock1(data.data() + off, le, b1)) return false;
  Located l;
  l.off = off;
  l.le = le;
  l.hashAt = hashAt;
  l.checkAt = checkAt;
  for (size_t i = 0; i < 5; i++) {
    l.hash[i] = get16(b1.data() + hashAt + 2 * i, le);
    l.check[i] = get16(b1.data() + checkAt + 2 * i, le);
  }
  if (!decodeStream(data, off + 512, 128, l.hash[0], l.check[0], le, l.block2)) return false;
  Reader b2(l.block2, le);
  int heroes = b2.u16(10), maxItem16 = b2.u16(46), maxTimers = b2.u16(28);
  if (heroes < 1 || heroes > 4) return false;
  if (maxItem16 > kMaxItem16 || maxTimers > kMaxTimers) return false;
  l.heroAt = off + 640 + 16 * size_t(maxItem16);
  for (auto &variant : kHeroVariants) {
    if (decodeStream(data, l.heroAt, variant[0], l.hash[2], l.check[2], le, l.heroes)) {
      l.heroRecord = variant[1];
      loc = std::move(l);
      return true;
    }
  }
  return false;
}

bool isExtended(const Bytes &data)
{
  return data.size() >= 512 && std::memcmp(data.data(), kSentinel, 20) == 0;
}

// End of a CSBwin prefix: header, data maps, game info, and the DSA level index
bool extendedEnd(const Bytes &data, size_t &end)
{
  if (!isExtended(data)) return false;
  Reader r(data, true);
  int32_t mapLength = r.i32(20);
  int flags = r.u8(37);
  int dsaCount = r.u16(38);
  uint32_t gameInfo = r.u32(44);
  if (mapLength < 0 || mapLength > 100000 || gameInfo > 100000) return false;
  if (dsaCount != 0) return false;   // the DSA records would need their own parser
  size_t off = 512 + 3 * size_t(mapLength) + gameInfo;
  if (flags & 0x01) {
    for (size_t e = off; e + 3 <= data.size() && e - off < 64 * 32 * 3 + 3; e += 3) {
      if (data[e] > 63) {
        if (data[e] == 0xff && data[e + 1] == 0xff && data[e + 2] == 0xff) {
          end = e + 3;
          return true;
        }
        return false;
      }
    }
    return false;
  }
  if (off > data.size()) return false;
  end = off;
  return true;
}

Located locate(const Bytes &data)
{
  if (data.size() < 640) throw FormatError("the file is too small for a savegame");
  const size_t layouts[2][2] = {{310, 342}, {312, 344}};   // hashes, checksums
  std::vector<size_t> first = {0};
  size_t ext = 0;
  if (extendedEnd(data, ext) && ext != 0) first.push_back(ext);
  size_t scanEnd = std::min(data.size() - 512, size_t(16384));
  Located loc;
  for (bool le : {false, true}) {
    for (size_t off : first)
      for (auto &l : layouts)
        if (tryChain(data, off, le, l[0], l[1], loc)) return loc;
    for (size_t off = 1; off <= scanEnd; off++) {
      if (off == ext) continue;
      for (auto &l : layouts)
        if (tryChain(data, off, le, l[0], l[1], loc)) return loc;
    }
  }
  throw FormatError("not a Dungeon Master or Chaos Strikes Back savegame");
}

// ------------------------------------------------------------ the dungeon
struct Dungeon {
  bool le = false;
  const Bytes *data = nullptr;
  size_t at = 0;                       // file offset of the dungeon
  int levels = 0;
  int counts[16] = {};
  int size[16] = {};                   // bytes of one record of each database
  size_t area[16] = {};                // file offset of each database
  size_t cells = 0;                    // file offset of the cell flags
  size_t columnIndex = 0, objectList = 0;
  int objectListLength = 0;
  // CSBwin: object references are indices into these maps
  bool indirect = false;
  const uint8_t *typeMap = nullptr;
  const uint8_t *indexMap = nullptr;
  size_t mapLength = 0;

  Reader file() const { return Reader(*data, le); }

  struct Ref {
    bool valid = false;
    int db = 0, index = 0;
    size_t record = 0;                 // file offset
  };
  Ref resolve(uint16_t rn) const
  {
    Ref r;
    if (rn >= 0xff00) return r;        // the special values: empty, end, spells
    if (indirect) {
      if (rn >= mapLength) return r;
      r.db = typeMap[rn] & 0xf;
      r.index = indexMap[2 * rn] | (indexMap[2 * rn + 1] << 8);
      if (r.index == 0xffff) return r;
    } else {
      r.db = (rn >> 10) & 0xf;
      r.index = rn & 0x3ff;
    }
    if (r.index >= counts[r.db]) return r;
    r.record = area[r.db] + size_t(r.index) * size_t(size[r.db]);
    r.valid = true;
    return r;
  }
};

Dungeon readDungeon(const Bytes &data, bool le, size_t at)
{
  Dungeon d;
  d.le = le;
  d.data = &data;
  d.at = at;
  Reader r = Reader(data, le).from(at);
  if (r.size() < 44) throw FormatError("the dungeon inside the savegame is cut short");
  d.levels = r.u16(4) >> 8;
  if (d.levels < 1 || d.levels > 64) throw FormatError("the dungeon has an impossible level count");
  size_t columns = 0;
  for (int i = 0; i < d.levels; i++) columns += ((r.u16(44 + 16 * size_t(i) + 8) >> 6) & 0x1f) + 1;
  d.objectListLength = r.u16(10);
  size_t textWords = r.u16(6);
  for (int i = 0; i < 16; i++) d.counts[i] = r.u16(12 + 2 * size_t(i));
  d.columnIndex = at + 44 + 16 * size_t(d.levels);
  d.objectList = d.columnIndex + 2 * columns;
  const size_t afterLists = 44 + 16 * size_t(d.levels) + 2 * columns +
                            2 * size_t(d.objectListLength);
  size_t cellBytes = isExtended(data) && data.size() >= 68 ? Reader(data, true).u32(48) : r.u16(2);
  // the databases, the cell flags and a checksum must end exactly at the file's end
  auto fits = [&](size_t o, const int *sizes) {
    size_t area[16];
    for (int db = 0; db < 16; db++) {
      area[db] = at + o;
      o += size_t(d.counts[db]) * size_t(sizes[db]);
    }
    if (o + cellBytes + 2 != r.size()) return false;
    for (int db = 0; db < 16; db++) {
      d.area[db] = area[db];
      d.size[db] = sizes[db];
    }
    d.cells = at + o;
    return true;
  };
  // the original games: the text as it is, actuators of 8 and scrolls of 4 bytes
  int classic[16];
  for (int db = 0; db < 16; db++) classic[db] = kDbSize[db];
  classic[3] = 8;
  classic[7] = 4;
  classic[11] = 8;
  if (fits(afterLists + 2 * textWords, classic)) return d;
  // CSBwin: a text index and the compressed text behind its length, which is usually in
  // the file's byte order (one lineage writes it the other way)
  size_t textAt = afterLists + 4 * textWords;
  for (bool textLe : {le, !le}) {
    if (textAt + 4 > r.size()) break;
    size_t textSize = Reader(data, textLe).u32(at + textAt);
    if (!fits(textAt + 4 + 2 * textSize, kDbSize)) continue;
    if (isExtended(data)) {
      Reader head(data, true);
      int32_t mapLength = head.i32(20);
      bool enciphered = (head.u8(37) & 0x04) != 0;
      if (mapLength > 0 && mapLength <= 100000 && !enciphered &&
          data.size() >= 512 + 3 * size_t(mapLength)) {
        d.indirect = true;
        d.mapLength = size_t(mapLength);
        d.typeMap = data.data() + 512;
        d.indexMap = data.data() + 512 + d.mapLength;
      } else {
        throw FormatError("the savegame's object maps cannot be read");
      }
    }
    return d;
  }
  throw FormatError("the dungeon inside the savegame has an unknown layout");
}

SavedItem describe(const Dungeon &d, uint16_t rn, const GameData &g)
{
  SavedItem item;
  Dungeon::Ref ref = d.resolve(rn);
  if (!ref.valid) return item;
  int word2 = d.file().u16(ref.record + 2);
  item.kind = ItemOther;
  switch (ref.db) {
    case 5:
      item.kind = ItemWeapon;
      item.type = word2 & 0x7f;
      item.name = weaponName(item.type);
      break;
    case 6:
      item.kind = ItemArmour;
      item.type = word2 & 0x7f;
      item.name = armourName(item.type);
      break;
    case 10:
      item.kind = ItemMisc;
      item.type = word2 & 0x7f;
      item.name = miscName(item.type);
      break;
    case 8:
      item.kind = ItemPotion;
      item.type = (word2 >> 8) & 0x7f;
      item.name = potionName(item.type);
      break;
    case 7: item.name = "Scroll"; break;
    case 9: item.name = "Chest"; break;
    default: item.name = "Object"; break;
  }
  (void)g;
  return item;
}

void readLevels(const Dungeon &d, SaveGame &save)
{
  Reader f = d.file();
  size_t column = 0;
  for (int i = 0; i < d.levels; i++) {
    size_t desc = d.at + 44 + 16 * size_t(i);
    SavedLevel level;
    int word8 = f.u16(desc + 8);
    level.width = ((word8 >> 6) & 0x1f) + 1;
    level.height = ((word8 >> 11) & 0x1f) + 1;
    level.expMultiplier = (f.u16(desc + 12) >> 12) & 0xf;
    size_t map = d.cells + f.u16(desc);
    for (int x = 0; x < level.width; x++, column++) {
      int next = f.u16(d.columnIndex + 2 * column);
      for (int y = 0; y < level.height; y++) {
        if (!(f.u8(map + size_t(x) * size_t(level.height) + size_t(y)) & 0x10)) continue;
        if (next >= d.objectListLength) throw FormatError("the dungeon's object list is short");
        uint16_t rn = f.u16(d.objectList + 2 * size_t(next++));
        for (int guard = 0; guard < 200; guard++) {
          Dungeon::Ref ref = d.resolve(rn);
          if (!ref.valid) break;
          if (ref.db == 4) {
            SavedGroup grp;
            grp.type = f.u8(ref.record + 4) & 0x1f;
            grp.count = ((f.u16(ref.record + 14) >> 5) & 3) + 1;
            for (int k = 0; k < 4; k++) grp.hp[k] = f.u16(ref.record + 6 + 2 * size_t(k));
            grp.x = x;
            grp.y = y;
            level.groups.push_back(grp);
          }
          rn = f.u16(ref.record);
        }
      }
    }
    save.levels.push_back(level);
  }
}

std::string text(const uint8_t *p, size_t max)
{
  std::string s;
  for (size_t i = 0; i < max && p[i]; i++) s += (p[i] >= 32 && p[i] < 127) ? char(p[i]) : '?';
  return s;
}

}  // namespace

int masteryFromExperience(int skill, const long experience[SkillCount])
{
  long e = experience[skill];
  if (skill > 3) e = (e + experience[(skill - 4) / 4]) >> 1;
  int mastery = 1;
  for (; e >= 500; mastery++) e >>= 1;
  return mastery;
}

SaveGame readSaveGame(const Bytes &data, const GameData &g, const std::string &label)
{
  Located loc = locate(data);
  const bool le = loc.le;
  SaveGame save;
  save.label = label;
  save.format = std::string(isExtended(data) ? "CSBwin save" : "classic save") +
                (le ? ", little-endian" : ", big-endian") +
                (loc.hashAt == 312 ? ", Chaos Strikes Back layout" : ", Dungeon Master layout");
  Reader b2(loc.block2, le);
  save.time = b2.u32(0);
  save.lastMoveTime = b2.u32(36);
  const int heroCount = b2.u16(10);
  save.x = b2.u16(12);
  save.y = b2.u16(14);
  save.facing = b2.u16(16);
  save.level = b2.u16(18);
  const int maxTimers = b2.u16(28);
  // the party's state follows the four hero records
  Reader hs(loc.heroes, le);
  const size_t tail = 4 * loc.heroRecord;
  save.party.partyShield = hs.i16(tail + 4);
  save.party.fireShield = hs.i16(tail + 6);
  save.party.spellShield = hs.i16(tail + 8);
  // the dungeon: after the timers and, where portraits are stored apart, after those
  Dungeon dungeon;
  bool haveDungeon = false;
  try {
    size_t timerSize = 10;
    if (loc.off != 0 && isExtended(data) && data.size() >= 68) {
      uint32_t flags = Reader(data, true).u32(64);
      if (flags & 0x80) timerSize = 16;        // ExtendedTimers
      else if (flags & 0x20) timerSize = 12;   // SequencedTimers
    }
    size_t at = loc.heroAt + loc.heroes.size() + timerSize * size_t(maxTimers) +
                2 * size_t(maxTimers) + (loc.heroes.size() == 1408 ? 4 * 464 : 0);
    dungeon = readDungeon(data, le, at);
    haveDungeon = true;
    readLevels(dungeon, save);
  } catch (const FormatError &e) {
    save.levels.clear();
    save.warnings.push_back(std::string("Levels and monsters could not be read: ") + e.what());
    if (!haveDungeon) save.warnings.push_back("Worn and held items could not be read.");
  }
  for (int i = 0; i < heroCount; i++) {
    Reader rec = hs.sub(size_t(i) * loc.heroRecord, loc.heroRecord);
    SavedHero sh;
    Hero &h = sh.hero;
    h.name = text(rec.data(), 8);
    sh.title = text(rec.data() + 8, 16);
    h.wounds = rec.u16(50);
    h.hp = rec.i16(52);
    h.maxHp = rec.i16(54);
    h.stamina = rec.i16(56);
    h.maxStamina = rec.i16(58);
    h.mana = rec.i16(60);
    h.maxMana = rec.i16(62);
    h.actionDefense = rec.i16(64);
    h.food = rec.i16(66);
    h.water = rec.i16(68);
    for (int k = 0; k < AttributeCount; k++) {
      h.attrMax[k] = rec.u8(70 + 3 * size_t(k));
      h.attr[k] = rec.u8(70 + 3 * size_t(k) + 1);
      h.attrMin[k] = rec.u8(70 + 3 * size_t(k) + 2);
    }
    for (int s = 0; s < SkillCount; s++)
      sh.experience[s] = long(rec.i16(92 + 6 * size_t(s))) + long(rec.i32(92 + 6 * size_t(s) + 2));
    for (int s = 0; s < SkillCount; s++) h.mastery[s] = masteryFromExperience(s, sh.experience);
    h.load = rec.u16(272);
    h.shield = rec.u16(274);
    // what is worn and held
    h.weapon = -1;
    if (haveDungeon) {
      for (int k = 0; k < SlotCount; k++) {
        try {
          sh.items[k] = describe(dungeon, rec.u16(212 + 2 * size_t(k)), g);
        } catch (const FormatError &) {
          sh.items[k] = SavedItem();
        }
      }
      for (int slot = 0; slot < 6; slot++) {
        const SavedItem &it = sh.items[slot];
        if (it.kind == ItemArmour && it.type < int(g.armours.size())) {
          h.armour[slot] = it.type;
          if (slot < 2) h.handWeight[slot] = g.armours[size_t(it.type)].weight;
        } else if (it.kind == ItemWeapon && it.type < int(g.weapons.size()) && slot < 2) {
          h.handWeight[slot] = g.weapons[size_t(it.type)].weight;
        }
      }
      const SavedItem &hand = sh.items[SlotActionHand], &neck = sh.items[SlotNeck];
      if (hand.kind == ItemWeapon && hand.type < int(g.weapons.size())) h.weapon = hand.type;
      h.elvenBoots = sh.items[SlotFeet].kind == ItemArmour &&
                     sh.items[SlotFeet].type == ArmourElvenBoots;
      h.ekkhardCross = neck.kind == ItemMisc && neck.type == 38;
      // DetermineMastery: what items add
      int all = 0;
      if (hand.kind == ItemWeapon && hand.type == WeaponFirestaffA) all = 1;
      if (hand.kind == ItemWeapon && hand.type == WeaponFirestaffB) all = 2;
      for (int &m : h.mastery) m += all;
      if (neck.kind == ItemMisc && neck.type == 41) h.mastery[SkillWizard]++;      // Pendant Feral
      if (neck.kind == ItemMisc && neck.type == 38) h.mastery[SkillDefend]++;      // Ekkhard Cross
      if ((neck.kind == ItemMisc && neck.type == 37) ||                            // Gem of Ages
          (hand.kind == ItemWeapon && hand.type == WeaponSceptreOfLyf))
        h.mastery[SkillHeal]++;
      if (neck.kind == ItemMisc && neck.type == 39) h.mastery[SkillInfluence]++;   // Moonstone
    }
    save.heroes.push_back(sh);
  }
  return save;
}

SaveGame loadSaveGame(const std::string &path, const GameData &g)
{
  Bytes data = readFile(path);
  std::string name = path;
  size_t slash = name.find_last_of("/\\");
  if (slash != std::string::npos) name = name.substr(slash + 1);
  if (looksLikeAdf(data)) {
    std::vector<AdfFile> files = adfFiles(data);
    int pick = adfPickSavegame(files);
    if (pick < 0) throw FormatError("the disk image holds no savegame (no .DAT file)");
    return readSaveGame(adfRead(data, files[size_t(pick)]), g,
                        files[size_t(pick)].name + " in " + name);
  }
  return readSaveGame(data, g, name);
}

}  // namespace calc
