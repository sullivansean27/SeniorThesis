import png
import math
import random

def lerp(a, b, t):
    return a + t*(b-a)

def fade(t):
	return t * t * t * (t * (t * 6.0 - 15.0) + 10.0)

def grad(hash, x, y):
    if hash & 4 == 0:
        return x + y
    elif hash & 4 == 1:
        return -x + y
    elif hash & 4 == 2:
        return x - y
    elif hash & 4 == 3:
        return -x - y
    else:
        return 0;

random.seed()
w = png.Writer(width=1024,height=1024,greyscale=True)
for k in range(1,20):
    randarr = [random.random() for i in range(0,256)]
    nums = [i for i in range(0,256)]
    pa = [a for (b,a) in sorted(zip(randarr,nums), key=lambda pair: pair[0])]

    perlinfile = open('noise/perlin'+str(k)+'.png','wb')
    perlin = []
    pretarr = []
    for ix in range(0,1024):
        tmp = []
        for iy in range(0,1024):
            total = 0
            freq = 1
            amp = 1
            persistence = 0.7;
            maxVal = 0
            for i in range(0,6):
                x = float(ix)/383*freq
                y = float(iy)/383*freq
                xi = int(x)%256
                yi = int(y)%256
                xf = x-float(xi)
                yf = y-float(yi)
                u = fade(xf)
                v = fade(yf)
                aa = pa[(pa[ xi    % 256]+  yi   ) % 256]
                ab = pa[(pa[ xi    % 256]+ (yi+1)) % 256]
                ba = pa[(pa[(xi+1) % 256]+  yi   ) % 256]
                bb = pa[(pa[(xi+1) % 256]+ (yi+1)) % 256]
                x1 = lerp(grad(aa, xf, yf), grad(ba,xf-1,yf), u)
                x2 = lerp(grad(ab, xf, yf-1), grad(bb,xf-1,yf-1), u)
                pret = lerp(x1,x2,v)+.5
                total = total + pret*amp
                maxVal = maxVal + amp
                amp = amp * persistence
                freq = freq * 2
            if total == maxVal:
                tmp.append(255)
            else:
                tmp.append(int(total/maxVal*256))
        perlin.append(tmp)
    w.write(perlinfile, perlin)
    perlinfile.close()
