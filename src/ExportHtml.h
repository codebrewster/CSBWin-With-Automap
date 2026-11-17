#ifndef EXPORTHTML_H
#define EXPORTHTML_H

// --export-html DIR (with --play): replays a playfile as fast as the machine allows, without
// drawing, snapshots or sound, lets the ending run (the fusion ticks and the end texts count in
// the statistics) and then writes DIR/map.html and DIR/statistics.html and exits.

bool ExportHtml_IsOn(void);
void ExportHtml_Enable(const char *dir);
// The replay's last line was read. The pages are written at once, unless the game is in its
// ending (then ExportHtml_OnGameEnd does it).
void ExportHtml_OnReplayEnd(void);
// The victory screen of the game is up: the ending is over
void ExportHtml_OnGameEnd(void);
// A message box nobody can answer: prints it and stops with exit code 2
void ExportHtml_OnMessageBox(const char *text);
// Called once per tick: gives up when the game runs on for ages after the replay's end
void ExportHtml_AtTick(void);

#endif
