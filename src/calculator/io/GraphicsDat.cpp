#include "GraphicsDat.h"

#include <cstdio>

namespace calc {

Bytes readFile(const std::string &path)
{
  FILE *f = std::fopen(path.c_str(), "rb");
  if (!f) throw FormatError("cannot open " + path);
  Bytes out;
  uint8_t buf[65536];
  size_t n;
  while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.insert(out.end(), buf, buf + n);
  std::fclose(f);
  return out;
}

namespace {

// LZWExpand / LZWGetNextCodeword / TAG022eec (Graphics.cpp): codes of 9 to 12
// bits read in groups of eight, a table reset at code 256, and on top of
// that a run-length layer with 0x90 as its escape.
class Lzw {
 public:
  Lzw(const uint8_t *src, size_t size) : src_(src), left_(size) {}

  Bytes expand()
  {
    std::vector<int> prefix(4096, 0);
    std::vector<uint8_t> suffix(4096, 0);
    for (int i = 0; i < 256; i++) suffix[size_t(i)] = uint8_t(i);
    std::vector<uint8_t> stack;
    nextCode_ = 257;
    int prev = next();
    if (prev < 0) return out_;
    int last = prev & 0xff;
    put(last);
    for (int code; (code = next()) > -1;) {
      if (code == 256) {
        clear_ = true;
        nextCode_ = 256;
        code = next();
        if (code == -1) break;
      }
      int cur = code;
      if (code >= nextCode_) {
        stack.push_back(uint8_t(last));
        code = prev;
      }
      while (code >= 256) {
        if (stack.size() > 8192) throw FormatError("graphics.dat: damaged item");
        stack.push_back(suffix[size_t(code)]);
        code = prefix[size_t(code)];
      }
      last = suffix[size_t(code)];
      stack.push_back(uint8_t(last));
      while (!stack.empty()) {
        put(stack.back());
        stack.pop_back();
      }
      if (nextCode_ < 4096) {
        prefix[size_t(nextCode_)] = prev;
        suffix[size_t(nextCode_)] = uint8_t(last);
        nextCode_++;
      }
      prev = cur;
    }
    return out_;
  }

 private:
  int next()
  {
    if (clear_ || bit_ >= bitsInBuffer_ || nextCode_ > maxCode_) {
      if (nextCode_ > maxCode_) {
        codeSize_++;
        maxCode_ = codeSize_ == 12 ? 4096 : (1 << codeSize_) - 1;
      }
      if (clear_) {
        codeSize_ = 9;
        maxCode_ = (1 << codeSize_) - 1;
        clear_ = false;
      }
      size_t want = left_ > size_t(codeSize_) ? size_t(codeSize_) : left_;
      if (want == 0) return -1;
      for (size_t i = 0; i < sizeof buffer_; i++) buffer_[i] = i < want ? src_[i] : 0;
      src_ += want;
      left_ -= want;
      bit_ = 0;
      bitsInBuffer_ = int(want * 8) - codeSize_ + 1;
    }
    unsigned v = 0;
    for (int i = 0; i < codeSize_; i++) {
      int b = bit_ + i;
      if (buffer_[b >> 3] & (1 << (b & 7))) v |= 1u << i;
    }
    bit_ += codeSize_;
    return int(v);
  }

  void put(int byte)
  {
    if (!repeat_) {
      if (byte == 0x90) {
        repeat_ = true;
      } else {
        repeatChar_ = uint8_t(byte);
        out_.push_back(uint8_t(byte));
      }
      return;
    }
    if (byte != 0) {
      for (int n = byte; --n != 0;) out_.push_back(repeatChar_);
    } else {
      out_.push_back(0x90);
    }
    repeat_ = false;
  }

  const uint8_t *src_;
  size_t left_;
  uint8_t buffer_[16] = {};
  int bit_ = 0, bitsInBuffer_ = 0;
  int codeSize_ = 9, maxCode_ = 511, nextCode_ = 257;
  bool clear_ = false;
  bool repeat_ = false;
  uint8_t repeatChar_ = 0;
  Bytes out_;
};

int bits(int word, int low, int count) { return (word >> low) & ((1 << count) - 1); }

}  // namespace

Bytes graphicsItem(const Bytes &file, int item)
{
  Reader r(file, false);
  size_t head = 2;
  int count = r.u16(0);
  if (count == 0x8001) throw FormatError("graphics.dat: little-endian files are not supported");
  if (count < 563 || count > 700) throw FormatError("not a graphics.dat with the game's tables");
  if (item < 0 || item >= count) throw FormatError("graphics.dat: no such item");
  size_t offset = head + size_t(count) * 4;
  for (int i = 0; i < item; i++) offset += r.u16(head + size_t(i) * 2);
  size_t packed = r.u16(head + size_t(item) * 2);
  size_t full = r.u16(head + size_t(count) * 2 + size_t(item) * 2);
  Reader body = r.sub(offset, packed);
  if (packed == full) return Bytes(body.data(), body.data() + packed);
  Bytes out = Lzw(body.data(), packed).expand();
  if (out.size() != full) throw FormatError("graphics.dat: an item did not expand to its size");
  return out;
}

// The items are images of the engine's data block; a field the engine names
// by its distance from the block's end sits at (end of item's range - name).
GameData readGraphicsDat(const Bytes &file, const std::string &label)
{
  GameData g;
  g.source = label;
  Bytes b22f = graphicsItem(file, 0x22f);
  Bytes b230 = graphicsItem(file, 0x230);
  Bytes b232 = graphicsItem(file, 0x232);
  if (b22f.size() != 0xc0e || b230.size() != 0x4e8 || b232.size() != 0x722)
    throw FormatError("graphics.dat: the game's tables have an unexpected size");
  Reader t(b22f, false);   // starts at Byte10340
  Reader a(b230, false);   // starts at Word20242
  Reader u(b232, false);   // starts at 1830
  auto at22f = [](int name) { return size_t(10340 - name); };
  auto at230 = [](int name) { return size_t(20242 - name); };
  auto at232 = [](int name) { return size_t(1830 - name); };

  // OBJDESC ObjDesc[180] at 8382: attackClass in byte 3, wearable slots in word 4
  const size_t objDesc = at22f(8382);
  auto attackClass = [&](int descIndex) { return int(t.u8(objDesc + size_t(descIndex) * 6 + 3)); };
  auto wearSlots = [&](int descIndex) { return int(t.u16(objDesc + size_t(descIndex) * 6 + 4)); };

  for (int i = 0; i < 27; i++) {   // MONSTERDESC MonsterDescriptor[27] at 9664
    size_t o = at22f(9664) + size_t(i) * 26;
    MonsterType m;
    m.id = i;
    m.name = monsterName(i);
    int word2 = t.u16(o + 2);
    m.size = bits(word2, 0, 2);
    m.hitsAnyHero = (word2 & 0x10) != 0;
    m.nonMaterial = (word2 & 0x40) != 0;
    m.invincible = (word2 & 0x2000) != 0;
    m.moveTicks = t.u8(o + 6);
    m.attackTicks = t.u8(o + 7);
    m.defense = t.u8(o + 8);
    m.baseHealth = t.u8(o + 9);
    m.attack = t.u8(o + 10);
    m.poison = t.u8(o + 11);
    m.dexterity = t.u8(o + 12);
    m.missileClass = bits(t.u16(o + 14), 12, 4);
    m.expClass = bits(t.u16(o + 16), 8, 4);
    m.fireResistance = bits(t.u16(o + 18), 4, 4);
    m.poisonResistance = bits(t.u16(o + 18), 8, 4);
    m.attackPause = bits(t.u16(o + 20), 0, 4);
    m.woundNibbles = t.u16(o + 22);
    m.attackKind = t.u8(o + 24);
    g.monsters.push_back(m);
  }
  for (int i = 0; i < 46; i++) {   // WEAPONDESC weapons[46] at 8658
    size_t o = at22f(8658) + size_t(i) * 6;
    WeaponType w;
    w.id = i;
    w.name = weaponName(i);
    w.weight = t.u8(o);
    w.cls = t.u8(o + 1);
    w.strength = t.u8(o + 2);
    w.distance = t.u8(o + 3);
    w.attribute = t.u16(o + 4);
    w.actionSet = attackClass(23 + i);
    g.weapons.push_back(w);
  }
  for (int i = 0; i < 58; i++) {   // CLOTHINGDESC ClothingDesc[58] at 8890
    size_t o = at22f(8890) + size_t(i) * 4;
    ArmourType c;
    c.id = i;
    c.name = armourName(i);
    c.weight = t.u8(o);
    c.defense = t.u8(o + 1);
    c.pierce = t.u8(o + 2);
    c.wearSlots = wearSlots(69 + i);
    g.armours.push_back(c);
  }
  static const char *const foodNames[8] = {"Apple", "Corn", "Bread", "Cheese", "Screamer Slice",
                                           "Worm Round", "Drumstick", "Dragon Steak"};
  for (int i = 0; i < 8; i++)      // FoodValue[8] at 8962
    g.foods.push_back({foodNames[i], t.i16(at22f(8962) + size_t(i) * 2), 0});
  g.foods.push_back({"Waterskin sip", 0, 800});            // Character.cpp:4195
  g.foods.push_back({"Water flask / fountain", 0, 1600});  // Character.cpp:4347, Mouse.cpp:1188

  static const int physical[] = {2, 6, 7, 9, 13, 14, 15, 16, 17, 18, 19, 24, 25, 28, 29, 30, 31};
  for (int i = 0; i < 44; i++) {
    ActionType act;
    act.id = i;
    act.name = actionName(i);
    act.exp = a.u8(at230(20178) + size_t(i));
    act.skill = a.u8(at230(20134) + size_t(i));
    act.defenseBonus = int8_t(a.u8(at230(20090) + size_t(i)));
    act.energy = a.u8(at230(20046) + size_t(i));
    act.hitChance = a.u8(at230(20002) + size_t(i));
    act.damage = a.u8(at230(19958) + size_t(i));
    act.ticks = a.u8(at230(19914) + size_t(i));
    for (int p : physical) act.physical = act.physical || p == i;
    g.actions.push_back(act);
    ActionSet set;                 // LEGALATTACKS legalAttacks[44] at 19570
    size_t o = at230(19570) + size_t(i) * 8;
    for (int k = 0; k < 3; k++) set.action[k] = a.u8(o + size_t(k));
    for (int k = 1; k < 3; k++) {
      int need = a.u8(o + size_t(k) + 3);
      set.needsCharge[k] = (need & 0x80) != 0;
      set.minMastery[k] = need & 0x7f;
    }
    g.actionSets.push_back(set);
  }
  static const char *const spellNames[25] = {
    "Party Shield", "Magic Footprints", "Invisibility", "Poison Cloud", "See Through Walls",
    "Lightning Bolt", "Light", "Torch", "Fireball", "Ku Potion (strength)", "Fire Shield",
    "Harm Non-Material", "Poison Bolt", "Darkness", "Open Door", "Ya Potion (shield)",
    "Mon Potion (stamina)", "Dane Potion (wisdom)", "Neta Potion (vitality)",
    "Vi Potion (health)", "Anti Venin", "Ros Potion (dexterity)", "Ee Potion (mana)",
    "Ven Potion (poison)", "Zo Kath Ra"};
  for (int i = 0; i < 25; i++) {   // SPELL Spells[25] at 19216
    size_t o = at230(19216) + size_t(i) * 8;
    SpellType s;
    s.id = i;
    s.name = spellNames[i];
    uint32_t id = a.u32(o);
    for (int k = 0; k < 3; k++) {
      int c = int((id >> (16 - 8 * k)) & 0xff);
      s.runes[k] = c == 0 ? 0 : c - (0x66 + 6 * k) + 1;
    }
    s.difficulty = a.u8(o + 4);
    s.skill = a.u8(o + 5);
    s.kind = bits(a.u16(o + 6), 0, 4);
    s.missile = bits(a.u16(o + 6), 4, 6);
    g.spells.push_back(s);
  }
  for (int p = 0; p < 6; p++) g.powerFactor[p + 1] = a.u8(at230(19016) + size_t(p));
  for (int row = 0; row < 4; row++)
    for (int k = 0; k < 6; k++) g.runeCost[row][k] = a.u8(at230(19010) + size_t(row) * 6 + size_t(k));
  for (int i = 0; i < 6; i++) g.slotProtection[i] = u.u8(at232(1412) + size_t(i));
  for (int i = 0; i < 4; i++) g.woundSlots[i] = u.u8(at232(590) + size_t(i));
  return g;
}

GameData loadGraphicsDat(const std::string &path)
{
  return readGraphicsDat(readFile(path), path);
}

}  // namespace calc
