//Sean Sullivan 2017

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <pthread.h>
#include <time.h>
#include <math.h>
#include "terrain.h"
#include "lodepng.h"

#define NO_AGENT 0
#define CST_AGENT 1
#define BEC_AGENT 2
#define MT_AGENT 3
#define HL_AGENT 4
#define RIV_AGENT 5
#define SM_AGENT 6

static short unsigned *terrain;
static short unsigned *terrainAgents;
static pthread_mutex_t pmutex;
static unsigned mtnHeightAvg = 10000;
static unsigned mtnHeightVar = 2000;
static unsigned mtnWidthAvg = 70;
static unsigned mtnWidthVar = 20;
static unsigned hillHeightAvg = 1200;
static unsigned hillHeightVar = 300;
static unsigned hillWidthAvg = 40;
static unsigned hillWidthVar = 10;
static unsigned limit = 1500;
static unsigned width = 1024;
static unsigned height = 1024;
static double sgauss;
static int gaussphase = 0;
static double gaussv1;
static double gaussv2;
static int curagent;

int xyToIndex(int x, int y){
  return x+y*width;
}

int randRange(int max){
  double r = (double) rand() / RAND_MAX;
  return (int) (r * max);
}

int randInRange(int min, int max){
  return randRange(max-min) + min;
}

double gaussRand() {
	//method due to Marsaglia found on c-faq.com/lib/gaussian.html
	//Box-Muller method http://www.design.caltech.edu/erik/Misc/Gaussian.html
	double X;
	if(gaussphase == 0){
		do{
			double u1 = (double) rand() / RAND_MAX;
			double u2 = (double) rand() / RAND_MAX;
			gaussv1 = 2*u1-1;
			gaussv2 = 2*u2-1;
			sgauss = gaussv1*gaussv1 + gaussv2*gaussv2;
		} while (sgauss >= 1 || sgauss == 0);
		X = gaussv1 * sqrt(-2*log(sgauss) / sgauss);
	} else {
		X = gaussv2 * sqrt(-2*log(sgauss) / sgauss);
	}
  gaussphase = 1 - gaussphase;
	return X;
}


int pointIsValid(int x, int y){
  if(x < 0 || y < 0 || x > width || y > height) return 0;
  else return 1;
}

int getDirection(int x1, int y1, int x2, int y2){
  if(x2 == x1) {
    if (y2 == y1) return -1;     //same point
    else if (y2 > y1) return 2;  //up
    else return 6;               //down
  }
  else if(x2 < x1){
    if(y2 == y1) return 4;       //left
    else if (y2 > y1) return 3;  //up-left
    else return 5;               //down-left
  }
  else {
    if(y2 == y1) return 0;       //right
    else if(y2 > y1) return 1;   //up-right
    else return 7;               //down-right
  }
}

int xTileInDirection(int px, int dir){
  switch(dir){
    case 0:
      return px+1;
    case 1:
      return px+1;
    case 3:
      return px-1;
    case 4:
      return px-1;
    case 5:
      return px-1;
    case 7:
      return px+1;
    default:
      return px;
  }
}

int yTileInDirection(int py, int dir){
  switch(dir){
    case 1:
      return py+1;
    case 2:
      return py+1;
    case 3:
      return py+1;
    case 5:
      return py-1;
    case 6:
      return py-1;
    case 7:
      return py-1;
    default:
      return py;
  }
}

void zigzagNextTile(int *px, int *py, int dir){
  if(dir%2 ==0){ //cardinals always move the right way
    *px = xTileInDirection(*px, dir);
    *py = yTileInDirection(*py, dir);
  } else{       //other dirs "zig-zag" to avoid holes
    int flip = randRange(2);
    if(flip) *px = xTileInDirection(*px, dir);
    else *py = yTileInDirection(*py, dir);
  }
}

int score(int px, int py, int ax, int ay, int rx, int ry){
  int da = (px - ax)*(px - ax) + (py - ay)*(py - ay);
  int dr = (px - rx)*(px - rx) + (py - ry)*(py - ry);
  int de = px;
  if(py < de) de = py;
  if(width-px < de) de = width - px;
  if(height-py < de) de = height - py;
  de = de * de;
  return dr-da+3*de;
}

int getTerrainMax(){
  int max = 0;
  for(int i = 0; i < height*width; i++){
    if(terrain[i] > max) max = terrain[i];
  }
  return max;
}

void findBorder(int *px, int *py, int dir){
  int x = *px;
  int y = *py;
  while(terrain[xyToIndex(x,y)] > 0){
    *px = x;
    *py = y;
    x = xTileInDirection(x, dir);
    y = yTileInDirection(y, dir);
  }
}

void findRandomLandPt(int *px, int *py){
  double r;
  do{
    *px = randRange(width);
    *py = randRange(height);
  } while(terrain[xyToIndex(*px,*py)] <= 0);
}

void findRandomBorderPt(int *px, int *py, int dir){
  findRandomLandPt(px, py);
  findBorder(px, py, dir);
}

void raiseWedge(int px, int py, int dir, int avgSteep, int mWidth, int agentID){
  int x = px;
  int y = py;
  //printf("New Wedge, s=%d W=%d\n", avgSteep, mWidth);
  if(pointIsValid(x,y)) {
    if(terrain[xyToIndex(x,y)] > 0){
      if(((signed)terrain[xyToIndex(x,y)]) + avgSteep*mWidth/2 > 0){
        terrain[xyToIndex(x,y)] += avgSteep*mWidth/2;
      } else{
        terrain[xyToIndex(x,y)] = 0;
      }
      terrainAgents[xyToIndex(x,y)] = agentID;
    }
  } else return;
  //CHECK IF AGENT==AGENTID IN CASE OF TURNS?
  for(int i = (mWidth-1)/2; i>0; i--){
    x = xTileInDirection(x,(dir+2)%8);
    y = yTileInDirection(y,(dir+2)%8);
    if(pointIsValid(x,y)) {
      if(terrain[xyToIndex(x,y)] > 0){
        if(((signed)terrain[xyToIndex(x,y)]) + avgSteep*i > 0){
          terrain[xyToIndex(x,y)] += avgSteep*i;
        } else{
          terrain[xyToIndex(x,y)] = 0;
        }
        terrainAgents[xyToIndex(x,y)] = agentID;
      } else break;
    } else break;
  }
  x = px;
  y = py;
  for(int i = (mWidth-1)/2; i>0; i--){
    x = xTileInDirection(x,(dir+6)%8);
    y = yTileInDirection(y,(dir+6)%8);
    if(pointIsValid(x,y)) {
      if(terrain[xyToIndex(x,y)] > 0){
        if(((signed)terrain[xyToIndex(x,y)]) + avgSteep*i > 0){
          terrain[xyToIndex(x,y)] += avgSteep*i;
        } else{
          terrain[xyToIndex(x,y)] = 0;
        }
        terrainAgents[xyToIndex(x,y)] = agentID;
      } else break;
    } else break;
  }
}

void flatten(int x, int y){
  unsigned tot = 0;
  unsigned count = 0;
  if(pointIsValid(x,y)){
    tot += terrain[xyToIndex(x,y)];
    count++;
  }
  if(pointIsValid(x+1,y)){
    tot += terrain[xyToIndex(x+1,y)];
    count++;
  }
  if(pointIsValid(x+1,y+1)){
    tot += terrain[xyToIndex(x+1,y+1)];
    count++;
  }
  if(pointIsValid(x,y+1)){
    tot += terrain[xyToIndex(x,y)];
    count++;
  }
  if(pointIsValid(x-1,y+1)){
    tot += terrain[xyToIndex(x-1,y+1)];
    count++;
  }
  if(pointIsValid(x-1,y)){
    tot += terrain[xyToIndex(x-1,y)];
    count++;
  }
  if(pointIsValid(x-1,y-1)){
    tot += terrain[xyToIndex(x-1,y-1)];
    count++;
  }
  if(pointIsValid(x,y-1)){
    tot += terrain[xyToIndex(x,y)];
    count++;
  }
  if(pointIsValid(x+1,y-1)){
    tot += terrain[xyToIndex(x+1,y-1)];
    count++;
  }
  terrain[xyToIndex(x,y)] = tot/(count+4);
}


int initAgent(int tokens){
  int px = width / 2;
  int py = height / 2;
  double r;
  int dir;
  srand(time(0) ^ (getpid()<<16));
  for(int i = 0; i < tokens; i++){
    //pthread_mutex_lock(&pmutex);
    terrain[xyToIndex(px,py)] = 200;
    //printf("x=%d y=%d h=%d\n", px, py, terrain[xyToIndex(px,py)]);
    //pthread_mutex_unlock(&pmutex);
    dir = randRange(8);
    px = xTileInDirection(px, dir);
    py = yTileInDirection(py, dir);
  }
  fflush(stdout);
  return 0;
}

int coastlineAgent(int tokens, int x0, int y0){ //while loop instead of recursion?
  if(tokens >= limit){
    //create child1
    int pid1 = fork();
    if(pid1==0){
      //child1 is a new agent
      srand(time(0) ^ (getpid()<<16));
      int dir = randRange(4);
      if(dir == 0) coastlineAgent(tokens/2, x0+1, y0);
      if(dir == 1) coastlineAgent(tokens/2, x0, y0+1);
      if(dir == 2) coastlineAgent(tokens/2, x0-1, y0);
      if(dir == 3) coastlineAgent(tokens/2, x0, y0-1);
    } else{
      //create child2
      int pid2 = fork();
      if(pid2==0){
        //child2 is a new agent
        srand(time(0) ^ (getpid()<<16));
        int dir = randRange(4);
        if(dir == 0) coastlineAgent(tokens/2, x0+1, y0);
        if(dir == 1) coastlineAgent(tokens/2, x0, y0+1);
        if(dir == 2) coastlineAgent(tokens/2, x0-1, y0);
        if(dir == 3) coastlineAgent(tokens/2, x0, y0-1);
      } else{
        //wait for the kids
        printf("%d waiting on children %d, %d, tokens=%d\n", getpid(), pid1, pid2, tokens/2);
        fflush(stdout);
        wait(&pid2);
        wait(&pid1);
        printf("%d is done waiting\n", getpid());
        fflush(stdout);
      }
    }
  }
  else{
    int px;
    int py;
    int dir = randRange(8);
    pthread_mutex_lock(&pmutex);
    findRandomBorderPt(&px, &py, dir);
    pthread_mutex_unlock(&pmutex);

    //new agents have a random attractor and repulsor
    int attrx = randRange(width);
    int attry = randRange(height);

    //if attry is opposite direction, that's bad
    int repx, repy;
    int dotp;
    //if dotp^2 > sqrt(2)*dist^2, then they are <45 degrees apart, so try again
    do {
      repx = randRange(width);
      repy = randRange(height);
      dotp = (attrx-px)*(repx-px)+(attry-py)*(repy-py);
    } while (dotp > 0);
    //printf("px:%d py:%d, initdir=%d, pid=%d\n", px, py, dir, getpid());
    int k = 0;
    while(k < tokens){
      pthread_mutex_lock(&pmutex);
      //for each p adjacent to point
      findRandomBorderPt(&px, &py, dir);
      int neighbors[8];
      int max = -1;
      for(int i = 0; i < 8; i++){
        int tmpx = xTileInDirection(px,i);
        int tmpy = yTileInDirection(py,i);
        if(pointIsValid(tmpx,tmpy)){
          if(terrain[xyToIndex(tmpx,tmpy)] == 0){
            neighbors[i] = score(tmpx,tmpy,attrx,attry,repx,repy);
            if(max == -1) max = i;
            else if(neighbors[i] > neighbors[max]) max = i;
          }
        }
      }
      if(max == -1){
        pthread_mutex_unlock(&pmutex);
        printf("Agent %d stopped with %d tokens left\n", getpid(), tokens-k);
        fflush(stdout);
        return 0;
      }
      px = xTileInDirection(px,max);
      py = yTileInDirection(py,max);
      terrain[xyToIndex(px, py)] += 200;
      terrainAgents[xyToIndex(px,py)] = CST_AGENT;
      //printf("h(%d,%d)=%d 0:%d 1:%d 2:%d 3:%d 4:%d 5:%d 6:%d 7:%d\n", px, py, terrain[xyToIndex(px, py)], neighbors[0], neighbors[1], neighbors[2], neighbors[3], neighbors[4], neighbors[5], neighbors[6], neighbors[7]);
      pthread_mutex_unlock(&pmutex);
      fflush(stdout);
      k++;
    }
    //beachAgent(100, px, py, 200);
    printf("Agent %d finished\n", getpid());
    fflush(stdout);
  }
  return 0;
}

int smoothingAgent(int tokens, int sx, int sy){
  int px = sy;
  int py = sy;
  int x,y;
  int avg;
  int count;
  double r;
  for(int i = 0; i < tokens; i++){
    //pthread_mutex_lock(&pmutex);
    if(pointIsValid(px,py)){
      avg = 3*terrain[xyToIndex(px,py)];
      count = 3;
      for(int k = 0; k < 4; k++){   //right,up,left,down with dir=k*2
        x = px; y = py;
        x = xTileInDirection(x,k*2);
        y = yTileInDirection(y,k*2);
        if(pointIsValid(x,y)){
          avg += terrain[xyToIndex(x,y)];
          count++;
          x = xTileInDirection(x,k*2);
          y = yTileInDirection(y,k*2);
          if(pointIsValid(x,y)){
            avg += terrain[xyToIndex(x,y)];
            count++;
          }
        }
      }
      terrain[xyToIndex(px,py)] = avg / count;
    }
    //pthread_mutex_unlock(&pmutex);
    int neighbor = randRange(9); //random neighbor
    if(neighbor != 8){ //8 = same tile; else = direction
      px = xTileInDirection(px, neighbor);
      py = yTileInDirection(py, neighbor);
    }
  }
}

int beachAgent(int tokens, int limit){
  int px, py;
  int edgeDir = randRange(8);
  findRandomBorderPt(&px, &py, edgeDir);
  int dir;
  int pathdir;
  for(int i=0; i<tokens; i++){
    dir = randRange(8); //random direction
    //pthread_mutex_lock(&pmutex);
    int count = 0;
    while((terrain[xyToIndex(px,py)] > limit)
        || (terrain[xyToIndex(xTileInDirection(px,dir),
                              yTileInDirection(py,dir))] < 0)) {
      findRandomBorderPt(&px, &py, edgeDir);
      dir = randRange(8);
      if(count++ > 20){
        return -1;
      }
    }
    //flatten
    //flatten(px,py);
    raiseWedge(px,py,dir,-2,3,BEC_AGENT);
    //smooth
    smoothingAgent(20, px, py);
    //find random point a short distance inward
    int inx, iny;
    int maxDist = 200;
    count = 0;
    do{
      findRandomLandPt(&inx, &iny);
      pathdir = getDirection(px,py,inx,iny);
    } while(((inx-px)*(inx-px)+(iny-py)*(iny-py) > maxDist));
          /*    || ((pathdir != (edgeDir+3)%8)        //three "inland" directions
                  && (pathdir != (edgeDir+4)%8)
                  && (pathdir != (edgeDir+5)%8)));*/
    //find path to beach
    while((pathdir = getDirection(inx,iny,px,py)) != -1){
      //flatten
      //flatten(inx,iny);
      count = 0;
      raiseWedge(inx,iny,pathdir,-2,5,BEC_AGENT);
      //smooth
      smoothingAgent(20, inx, iny);
      //next tile in path
      zigzagNextTile(&inx,&iny,pathdir);
      if(count++ > maxDist){
        break;
      }
    }
    //pthread_mutex_unlock(&pmutex);
    //next point in dir

    px = xTileInDirection(px,dir);
    py = yTileInDirection(py,dir);
  }
}

//ADD ZIGZAG TO DIAG DIRS AND REMOVE +-H PER WEDGE
//MAYBE ADD PHASE (+/-) FOR H/W CHANGE FOR "MOUNTAIN" EFFECT
int mountainAgent(int tokens){
  int px, py;
  findRandomLandPt(&px, &py);
  int dir = randRange(8);
  int hillLikelihood = 50;
  int dirChangeFreq = 2;
  int flip;
  unsigned h = randInRange(mtnHeightAvg-mtnHeightVar,mtnHeightAvg+mtnHeightVar);
  unsigned w = randInRange(mtnWidthAvg-mtnWidthVar,mtnWidthAvg+mtnWidthVar);
  int wcur = 1;
  while(wcur < w){
    if(terrain[xyToIndex(px,py)] > 0){
      raiseWedge(px, py, dir, h/w, wcur, MT_AGENT);
      zigzagNextTile(&px,&py,dir);
    } wcur += randRange(3);
  }
  for(int i=0; i<tokens; i++){
    printf("p=(%d,%d), h=%u, w=%u\n", px, py, h, w);
    if(terrain[xyToIndex(px,py)] > 0){
      int tmp = h;
      do {
        tmp += (int)(gaussRand()*mtnHeightVar/8);
      } while(tmp <= 0);
      h = tmp;
      tmp = w;
      do {
        tmp += (int)(gaussRand()*mtnWidthVar/8);
      } while(tmp <= 0);
      w = tmp;

      //elevate wedge perpendicular to direction
      if(w>0){
        raiseWedge(px, py, dir, h/w, w, MT_AGENT);
      }
      /*if(randRange(hillLikelihood) == 0) {
        fprintf(stderr, "make a foothill\n"); fflush(stderr);
        int tmpx = px;
        int tmpy = py;
        int tmpd = (((dir+2)%8)+randRange(2)*4)%8;
        for(int k=0; k<w; k++){
          tmpx = xTileInDirection(tmpx, tmpd);
          tmpy = yTileInDirection(tmpy, tmpd);
        }
        hillAgent(tokens/30, tmpx, tmpy, tmpd);
      }*/
      //smooth
      smoothingAgent(w, px, py);
      //location <- next point in direction
      zigzagNextTile(&px,&py,dir);
      //"about every nth token", dir = (dir +/- 1) % 8
      if(randRange(tokens/dirChangeFreq) == 0){
        flip = randRange(2);
        if(flip) dir = (dir + 1) % 8;
        else dir = (dir + 7) % 8;
      }
    } else {
      fprintf(stderr, "unknown territory in mountainland oldDir=%d ", dir);
      int r1x = xTileInDirection(px, (dir+3)%8);
      int r1y = yTileInDirection(py, (dir+3)%8);
      int r2x = xTileInDirection(px, (dir+2)%8);
      int r2y = yTileInDirection(py, (dir+2)%8);
      int l1x = xTileInDirection(px, (dir+5)%8);
      int l1y = yTileInDirection(py, (dir+5)%8);
      int l2x = xTileInDirection(px, (dir+6)%8);
      int l2y = yTileInDirection(py, (dir+6)%8);
      if(terrain[xyToIndex(r1x,r1y)] > 0 && terrain[xyToIndex(l1x,l1y)] > 0){
        flip = randRange(2);
        if(flip) {
          px = xTileInDirection(px, (dir+3)%8);
          py = yTileInDirection(py, (dir+3)%8);
          dir = (dir + 1) % 8;
        }
        else {
          px = xTileInDirection(px, (dir+5)%8);
          py = yTileInDirection(py, (dir+5)%8);
          dir = (dir + 7) % 8;
        }
      } else if(terrain[xyToIndex(r1x,r1y)] > 0){
        px = xTileInDirection(px, (dir+3)%8);
        py = yTileInDirection(py, (dir+3)%8);
        dir = (dir + 1) % 8;
      } else if(terrain[xyToIndex(l1x,l1y)] > 0){
        px = xTileInDirection(px, (dir+5)%8);
        py = yTileInDirection(py, (dir+5)%8);
        dir = (dir + 7) % 8;
      } else if(terrain[xyToIndex(r2x,r2y)] > 0 && terrain[xyToIndex(l2x,l2y)] > 0){
        flip = randRange(2);
        if(flip) {
          dir = (dir + 2) % 8;
          px = xTileInDirection(px, dir);
          py = yTileInDirection(py, dir);
        }
        else {
          dir = (dir + 6) % 8;
          px = xTileInDirection(px, dir);
          py = yTileInDirection(py, dir);
        }
      } else if(terrain[xyToIndex(r2x,r2y)] > 0){
        dir = (dir + 2) % 8;
        px = xTileInDirection(px, dir);
        py = yTileInDirection(py, dir);
      } else if(terrain[xyToIndex(l2x,l2y)] > 0){
        dir = (dir + 6) % 8;
        px = xTileInDirection(px, dir);
        py = yTileInDirection(py, dir);
      } else {
        break;
      }
      fprintf(stderr, "newdir=%d\n", dir); fflush(stderr);
    }
  }
  while(wcur > 0){
    if(terrain[xyToIndex(px,py)] > 0){
      raiseWedge(px, py, dir, h/w, wcur, MT_AGENT);
      zigzagNextTile(&px,&py,dir);
    } wcur -= randRange(3);
  }
}

int hillAgent(int tokens, int sx, int sy, int dir){
  int px = sx;
  int py = sy;
  unsigned h = randInRange(hillHeightAvg-hillHeightVar,hillHeightAvg+hillHeightVar);
  unsigned w = randInRange(hillWidthAvg-hillWidthVar,hillWidthAvg+hillWidthVar);
  for(int i=0; i<tokens; i++){
    int tmp = h;
    do {
      tmp += (int)(gaussRand()*hillHeightVar/4);
    } while(tmp <= 0);
    h = tmp;
    tmp = w;
    do {
      tmp += (int)(gaussRand()*hillWidthVar/4);
    } while(tmp <= 0);
    w = tmp;
    smoothingAgent(w, px, py); //height[loc] = weight avg of neighborhood
    raiseWedge(px, py, dir, h/w, w, HL_AGENT);
    zigzagNextTile(&px,&py,dir);
  }
}

int riverAgent(){
  int mx, my, bx, by;
  int dir = randRange(8);
  findRandomBorderPt(&bx,&by,dir);
  do {
    findRandomLandPt(&mx,&my);
  } while (terrainAgents[xyToIndex(mx,my)] != MT_AGENT);
  int rWidth = randInRange(10,20);
  int px = mx;
  int py = my;
  while((dir = getDirection(px,py,bx,by)) != -1){
    //flatten wedge perpendicular to downhill
    raiseWedge(px,py,dir,-20,rWidth,RIV_AGENT);
    //smooth area around point
    smoothingAgent(5,px,py);
    //point <- next point in path
    zigzagNextTile(&px,&py,dir);
  }
  return 0;
}

int runRandomAgent(int tokens, int sw, int bw, int mw, int hw, int rw){
  int r = randRange(sw+bw+mw+hw+rw);
  int x0 = sw;
  int x1 = x0+bw;
  int x2 = x1+mw;
  int x3 = x2+hw;
  int x4 = x3+rw;
  if(r < x0){
    curagent = SM_AGENT;
    smoothingAgent(tokens, randRange(width), randRange(height));
    return SM_AGENT;
  }
  if(r < x1){
    curagent = MT_AGENT;
    mountainAgent(randInRange(tokens,tokens*2));
    return MT_AGENT;
  }
  if(r < x2){
    curagent = HL_AGENT;
    int x,y;
    findRandomLandPt(&x,&y);
    hillAgent(tokens/8,x,y,randRange(8));
    return HL_AGENT;
  }
  if(r < x3){
    curagent = BEC_AGENT;
    beachAgent(tokens/5,600);
    return BEC_AGENT;
  }
  if(r < x4){
    curagent = RIV_AGENT;
    int c = 0;
    while(riverAgent()==-1){
      c++;
      if (c > 30) {
        return -1;
      }
    }
    return RIV_AGENT;
  }
  return -1;
}


void exportTerrain(const char* pngFileName, const char *texFileName){
  unsigned char * image = malloc(sizeof(unsigned char)*width*height);
  unsigned char * id = malloc(sizeof(unsigned char)*width*height);
  int max = getTerrainMax();
  unsigned short out;
//  printf("TerrainMax = %d\n", max);
//  FILE *img = fopen(rawFileName, "w");
//  if(img == NULL) printf("Raw file not created\n");
  for(int i = 0; i < width*height; i++){
    //id[i*2] = (unsigned char) ((terrainAgents[i]) >> 8);
    //id[i*2+1] = (unsigned char) terrainAgents[i];
    //image[i*2] = (unsigned char) (out >> 8);
    //image[i*2+1] = (unsigned char) out;
    id[i] = (unsigned char) terrainAgents[i];
    out = (int)((double)terrain[i]/max*256);
    image[i] = (unsigned char) out;
//    fputc((int)image[i*2], img); fputc((int)image[i*2+1], img);
  }
  //fclose(img);
  pngEncodeTerrainMap(pngFileName,image);
  pngEncodeTerrainMap(texFileName,id);
  free(image);
  free(id);
}

void pngEncodeTerrainMap(const char* filename, const unsigned char* image){
  /*Encode the image*/
  unsigned error = lodepng_encode_file(filename, image, width, height, LCT_GREY, 8);
  /*if there's an error, display it*/
  if(error) printf("error %u: %s\n", error, lodepng_error_text(error));
}

//UNNECESSARY
/*
void exportOBJTerrain(const char *objFileName){
  FILE *obj = fopen(objFileName, "w+");
  if(obj == NULL) printf("OBJ file not created\n");
  fprintf(obj, "# OBJ file: terrain.obj");
  for(int i=0; i<width; i++) for(int j=0; j<height; j++){
    fprintf(obj, "v %f %f %f\n", (double)i,(double)terrain[xyToIndex(i,j)],(double)j);
  }
  for(int i=0; i<width; i++) for(int j=0; j<height; j++){
    fprintf(obj, "vt %f %f\n", (double)i/width, (double)j/height);
  }
  for(int i=0; i<width; i++) for(int j=0; j<height; j++){
    fprintf(obj, "vn 0.0 1.0 0.0\n");
  }
  for(int i=0; i<width; i++) {
    for(int j=0; j<height; j++){
      if(pointIsValid(i+1,j)){
        if(pointIsValid(i+1,j+1)){
          fprintf(obj, "f %d/%d/%d %d/%d/%d %d/%d/%d\n",
                    xyToIndex(i,j)+1,xyToIndex(i,j)+1,xyToIndex(i,j)+1,
                    xyToIndex(i+1,j)+1,xyToIndex(i+1,j)+1,xyToIndex(i+1,j)+1,
                    xyToIndex(i+1,j+1)+1,xyToIndex(i+1,j+1)+1,xyToIndex(i+1,j+1)+1);
        }
        if(pointIsValid(i+1,j-1)){
          fprintf(obj, "f %d/%d/%d %d/%d/%d %d/%d/%d\n",
                    xyToIndex(i,j)+1,xyToIndex(i,j)+1,xyToIndex(i,j)+1,
                    xyToIndex(i+1,j)+1,xyToIndex(i+1,j)+1,xyToIndex(i+1,j)+1,
                    xyToIndex(i+1,j-1)+1,xyToIndex(i+1,j-1)+1,xyToIndex(i+1,j-1)+1);
        }
      }
      if(pointIsValid(i,j+1)){
        if(pointIsValid(i+1,j+1)){
          fprintf(obj, "f %d/%d/%d %d/%d/%d %d/%d/%d\n",
                  xyToIndex(i,j)+1,xyToIndex(i,j)+1,xyToIndex(i,j)+1,
                  xyToIndex(i,j+1)+1,xyToIndex(i,j+1)+1,xyToIndex(i,j+1)+1,
                  xyToIndex(i+1,j+1)+1,xyToIndex(i+1,j+1)+1,xyToIndex(i+1,j+1)+1);
        }
        if(pointIsValid(i-1,j+1)){
          fprintf(obj, "f %d/%d/%d %d/%d/%d %d/%d/%d\n",
                  xyToIndex(i,j)+1,xyToIndex(i,j)+1,xyToIndex(i,j)+1,
                  xyToIndex(i,j+1)+1,xyToIndex(i,j+1)+1,xyToIndex(i,j+1)+1,
                  xyToIndex(i-1,j+1)+1,xyToIndex(i-1,j+1)+1,xyToIndex(i-1,j+1)+1);
        }
      }
      if(pointIsValid(i-1,j)){
        if(pointIsValid(i-1,j+1)){
          fprintf(obj, "f %d/%d/%d %d/%d/%d %d/%d/%d\n",
                    xyToIndex(i,j)+1,xyToIndex(i,j)+1,xyToIndex(i,j)+1,
                    xyToIndex(i-1,j)+1,xyToIndex(i-1,j)+1,xyToIndex(i-1,j)+1,
                    xyToIndex(i-1,j+1)+1,xyToIndex(i-1,j+1)+1,xyToIndex(i-1,j+1)+1);
        }
        if(pointIsValid(i-1,j-1)){
          fprintf(obj, "f %d/%d/%d %d/%d/%d %d/%d/%d\n",
                    xyToIndex(i,j)+1,xyToIndex(i,j)+1,xyToIndex(i,j)+1,
                    xyToIndex(i-1,j)+1,xyToIndex(i-1,j)+1,xyToIndex(i-1,j)+1,
                    xyToIndex(i-1,j-1)+1,xyToIndex(i-1,j-1)+1,xyToIndex(i-1,j-1)+1);
        }
      }
      if(pointIsValid(i,j-1)){
        if(pointIsValid(i+1,j-1)){
          fprintf(obj, "f %d/%d/%d %d/%d/%d %d/%d/%d\n",
                    xyToIndex(i,j)+1,xyToIndex(i,j)+1,xyToIndex(i,j)+1,
                    xyToIndex(i,j-1)+1,xyToIndex(i,j-1)+1,xyToIndex(i,j-1)+1,
                    xyToIndex(i+1,j-1)+1,xyToIndex(i+1,j-1)+1,xyToIndex(i+1,j-1)+1);
        }
        if(pointIsValid(i-1,j-1)){
          fprintf(obj, "f %d/%d/%d %d/%d/%d %d/%d/%d\n",
                    xyToIndex(i,j)+1,xyToIndex(i,j)+1,xyToIndex(i,j)+1,
                    xyToIndex(i,j-1)+1,xyToIndex(i,j-1)+1,xyToIndex(i,j-1)+1,
                    xyToIndex(i-1,j-1)+1,xyToIndex(i-1,j-1)+1,xyToIndex(i-1,j-1)+1);
        }
      }
    }
  }
  //close the mesh bottom somehow?
  fclose(obj);
}
*/

int main(int argc, char **argv){
  /* Initialize attribute to mutex.*/
  pthread_mutexattr_t terrain_attr_mutex;
  pthread_mutexattr_init(&terrain_attr_mutex);
  pthread_mutexattr_setpshared(&terrain_attr_mutex, PTHREAD_PROCESS_SHARED);

  if (pthread_mutex_init(&pmutex, &terrain_attr_mutex) != 0) {
        printf("\n pmutex init failed\n");
        return 1;
  }
  terrain = mmap(NULL, sizeof(terrain)*height*width, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
  terrainAgents = mmap(NULL, sizeof(terrainAgents)*height*width, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
  for(int i = 0; i < width; i++){
    for(int j = 0; j < height; j++){
      terrain[xyToIndex(i,j)] = 0.0;
    }
  }
  srand(time(0) ^ (getpid()<<16));
  initAgent(height*width/1000);
  int tokens = randInRange(400000,700000);
  fprintf(stderr,"coastline time\n"); fflush(stderr);
  int coastpid = fork();
  if(coastpid==0){
    coastlineAgent(tokens, width/2, height/2);
  } else{
    wait(&coastpid);
    pthread_mutex_destroy(&pmutex);
    pthread_mutexattr_destroy(&terrain_attr_mutex);
    tokens = (int)sqrt(tokens)/10;
    int agent;
    int numMtns = 0; //to scale with river likelihood
    for(int i = 0; i < 40; i++){
      //fork??
      int agent = runRandomAgent(tokens,40,30,10,20,numMtns*3);
      if (agent == MT_AGENT) numMtns++;
      fprintf(stderr, "agent %d: %d\n",i,agent);
    }

    //csv output
    /*
    FILE *out = fopen(argv[1], "w");
    if(out == NULL) {
      printf("File not opened properly\n");
      return 1;
    } else { printf("Creating file...\n"); }
    for(int i = 0; i < height; i++){
      for(int j = 0; j < width; j++){
        if(fprintf(out, "%d,", terrain[xyToIndex(j,i)]) < 0){
          printf("Error writing to file\n");
        }
      }
      if(fprintf(out, "\n") < 0){
        printf("Error writing to file\n");
      }
    }
    */
    printf("Exporting images...\n");
    exportTerrain(argv[1], argv[2]);
    //exportOBJTerrain("terrain.obj");
    munmap(terrain, sizeof(terrain)*height*width);
    munmap(terrainAgents, sizeof(terrainAgents)*height*width);
  }
  return 0;
}
