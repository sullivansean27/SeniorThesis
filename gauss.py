import png
import math
import random

phase = 0;
random.seed();
w = png.Writer(width=1024,height=1024,greyscale=True)
for k in range(1,20):
    gaussfile = open('noise/gauss'+str(k)+'.png','wb')
    gauss = []
    c = 0.0
    v1 = 0.0
    v2 = 0.0
    s = 0.0
    for y in range(0,1024):
        tmp = []
        for x in range(0,1024):
            if phase == 0:
                while True:
                    v1 = 2*random.random()-1
                    v2 = 2*random.random()-1
                    s = v1*v1 + v2*v2
                    if s < 1.0 and s != 0.0:
                        break
                c = v1 * math.sqrt(-2*math.log(s) / s)
            else:
                c = v2 * math.sqrt(-2*math.log(s) / s)
            c = c*0.15+0.5
            if c > 1.0:
                tmp.append(255)
            elif c < 0.0:
                tmp.append(0)
            else:
                tmp.append(int(c*256))
        gauss.append(tmp)
    w.write(gaussfile, gauss)
    gaussfile.close()
