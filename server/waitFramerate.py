import time

#Timing stuff
lastTime = None
maxFrameTime = 0;

def waitFramerate(T): #TODO if we have enough time, call the garbage collector
	global lastTime, maxFrameTime
	ctime = time.monotonic()
	if lastTime:
		frameTime = ctime-lastTime #how long the last frame took
		sleepTime = T-frameTime #how much time is remaining in target framerate
		if frameTime > maxFrameTime:
			maxFrameTime = frameTime
			print("Frame took "+str(maxFrameTime))
		if(sleepTime <= 0): #we went overtime. set start of next frame to now, and continue
			lastTime = ctime
		else:
			lastTime = lastTime+T
			time.sleep(sleepTime)
	else:
		lastTime = ctime
