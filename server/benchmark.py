import time

import obj
from placement import Placement
from placement import Quat
import assetManifest
from hypercubeCollide import hypercol

manifest = assetManifest.readManifest("assets/manifest3.json")
collide = hypercol.Hypercol(3)#request a 3D collider
for x in manifest["models"]:
	collide.loadOClass(x['name'], "assets/"+x['name']+".nhc3")

def testcase1():
	myscene = collide.newScene([[1]])
	collide.selectScene(myscene)
	for x in range(2):
		oinst = collide.newOInstance("humanFighter", 0, collide.createPoint((0,0,0)), collide.createOrientation(1,0,0,0), 1.0)
		collide.addInstance(oinst)
		oinst = collide.newOInstance("humanFighter", 0, collide.createPoint((100000,0,0)), collide.createOrientation(1,0,0,0), 1.0)
		collide.addInstance(oinst)
	collide.calculateScene()
	c = collide.getCollisions()
	collide.freeScene(myscene)
	collisionCount = c[0]
	#print("Collisions:",collisionCount)

t1 = time.time()
for x in range(30):
	testcase1()
t2 = time.time()
print("Timedelta",t2-t1)
