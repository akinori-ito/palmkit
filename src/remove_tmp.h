/* callback routine for signalling */
RETSIGTYPE scavenger_cb(int sig);
void RemoveAfter(char *file);
void DoCleanup();
void TrapSignalsToScavenge(int sig1, ...);
void TrapAllSignals();
