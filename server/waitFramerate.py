import time

#Timing stuff
lastTime = None
prevFrameTime = 0;

def waitFramerate(T): #TODO if we have enough time, call the garbage collector
	global lastTime, prevFrameTime
	ctime = time.monotonic()
	if lastTime:
		frameTime = ctime-lastTime #how long the last frame took
		sleepTime = T-frameTime #how much time is remaining in target framerate
		if prevFrameTime > frameTime and prevFrameTime > 1.2*T:
			print("Peak frame took "+str(prevFrameTime)[:5]+"/"+str(int(1.0/prevFrameTime))+" FPS (Target "+str(T)[:5]+")")
		if(sleepTime <= 0): #we went overtime. set start of next frame to now, and continue
			lastTime = ctime
		else:
			lastTime = lastTime+T
			time.sleep(sleepTime)
		prevFrameTime = frameTime
	else:
		lastTime = ctime
