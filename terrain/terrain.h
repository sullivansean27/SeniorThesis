#ifndef TERRAIN_H
#define TERRAIN_H

int randRange(int max);
int randInRange(int min, int max);

int xyToIndex(int x, int y);
int pointIsValid(int x, int y);
int getDirection(int px, int py, int ax, int ay);
int xTileInDirection(int px, int dir);
int yTileInDirection(int py, int dir);

int score(int px, int py, int ax, int ay, int rx, int ry);

int getTerrainMax();

void findBorder(int *px, int *py, int dir);
void findRandomLandPt(int *px, int *py);
void findRandomBorderPt(int *px, int *py, int dir);
void raiseWedge(int px, int py, int dir, int as, int w, int id);

int initAgent(int tokens);
int coastlineAgent(int tokens, int x0, int y0);
int smoothingAgent(int tokens, int x0, int y0);
int beachAgent(int tokens, int limit);
int mountainAgent(int tokens);
int hillAgent(int tokens, int sx, int sy, int d);
int riverAgent();

int runRandomAgent(int tokens, int sw, int bw, int mw, int hw, int rw);

void pngEncodeTerrainMap(const char* filename, const unsigned char* image);
void exportTerrain(const char* fn1, const char* fn2, const char* fn3);

#endif
