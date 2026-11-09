# Damage calculator (C++ / Qt)

Port of `damage_calc_gui.py` with a multi-threaded Monte Carlo and a
side-by-side check of the exact calculation against it.

    cmake -S . -B build && cmake --build build -j
    ./build/damage_calc_qt                       # GUI (Qt5 or Qt6: Widgets, Concurrent)
    ./build/damage_cli [scenarios] [trials]      # self-test, no Qt needed

* `DamageEngine.h` - the engine's formulas (Attack.cpp / Character.cpp /
  Code11f52.cpp) as integer code with C semantics; the Monte Carlo runs it.
* `DamageExact.cpp` - exact distribution: the same steps with probability
  tables; stamina and luck are a joint Markov state from swing to swing.
* `DamageMonteCarlo.cpp` - all cores, fixed batches (result independent of
  the thread count), standard errors from the batch spread.
* `DamageData.cpp` is generated: `python3 gen_data.py` (tables of `../damage_calc.py`).
* "Load from save..." runs `dump_champions.py` (needs python3) on `../savefile.py`.

Model options (checkboxes): stamina drain, luck drift (IsCharacterLucky
moves Luck by +-2 per call), STRandom modulo bias, monster attacking.
Stamina/luck restart every "swings per fight"; 0 = one long sequence.
