#ifndef EXPORTHTML_H
#define EXPORTHTML_H

// --export-html DIR (with --play): replays a playfile as fast as the machine allows, without
// drawing, snapshots or sound, lets the ending run (the fusion ticks and the end texts count in
// the statistics) and then writes DIR/map.html and DIR/statistics.html and exits.

bool ExportHtml_IsOn(void);
void ExportHtml_Enable(const char *dir);
// --export-ending (with --export-html): the pages wait for the victory screen, so the ending's
// moves (Chaos Strikes Back teleports the party after the replay's last line) are in the map.
// Without it the pages are written at the replay's last line, as Dungeon Master needs
void ExportHtml_WaitForEnding(void);
// The replay's last line was read. The pages are written at once, unless the game is in its
// ending or --export-ending was given (then ExportHtml_OnGameEnd does it, or the give-up).
void ExportHtml_OnReplayEnd(void);
// The victory screen of the game is up: the ending is over
void ExportHtml_OnGameEnd(void); // also the party dead (the lost screen)
// A message box nobody can answer: prints it and stops with exit code 2
void ExportHtml_OnMessageBox(const char *text);
// Called once per tick and in the waits of the ending: gives up when the game runs on for ages
// after the replay's end (in ticks or in seconds)
void ExportHtml_AtTick(void);

#endif
