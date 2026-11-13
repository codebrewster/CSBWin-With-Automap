#ifndef REPLAYSNAPSHOT_H
#define REPLAYSNAPSHOT_H

// Snapshots of a replay, for going back in it. The engine runs only forward; a snapshot holds
// all of its game state at the end of a tick, so a replay can be put back to that tick and run
// on from there. They are taken in the engine's main loop between two ticks (the one place
// where the engine's own call stack is the same every time) and put back there too.

#include <cstddef>

// Every 250 ticks of a replay, and where it began
const int REPLAY_SNAPSHOT_EVERY = 250;

// The main loop calls this between two ticks. During a replay it takes the snapshots and puts
// one back when PlaybackControl asks for a rewind (PlaybackControl_RewindBy).
void ReplaySnapshot_AtTickBoundary(void);
// A replay opens: the snapshots of the one before are gone
void ReplaySnapshot_Forget(void);
int ReplaySnapshot_Count(void);
size_t ReplaySnapshot_Bytes(void); // what they take, compressed
int ReplaySnapshot_First(void); // the tick of the earliest, -1 when none

// --rewind-test: every stretch of a replay between two snapshots runs twice, the second time
// from the snapshot taken at its start. Where the two runs end up differently, it names the
// variables. It reports on stdout and ends the program when the replay ends.
void ReplaySnapshot_EnableTest(void);
// --rewind-check 5000:2500,30000:250,end:2500 goes back 2500 ticks at tick 5000 and 250 at 30000 as the
// keys would; 5000:-2500 runs 2500 ticks ahead. "none" goes nowhere. At the end it prints a
// fingerprint of the game's state.
void ReplaySnapshot_EnableCheck(const char *presses);
bool ReplaySnapshot_TestIsRunning(void);
// Going back n ticks as the buttons do (from the tick the game stands at; from the tick a replay
// ended at when it is over: it opens again then). False when there is nothing to go back to.
bool ReplaySnapshot_BackFromEnd(int n, int speedBefore);
// The engine's message loop calls this between two messages. After a win or a death, a rewind that
// waits there puts the engine back into its main loop, which then puts the snapshot back.
void ReplaySnapshot_BetweenMessages(void);
// A replay's rewind waits (for the end sequence of a win, which blocks the engine's loop)
bool ReplaySnapshot_EndScreenWaits(void);

// The replay is over or the game is won (ReplayBackToNormalSpeed): a test or check ends here,
// since the end screen runs at 1x and reaches no tick boundary
void ReplaySnapshot_ReplayOver(void);

#endif
