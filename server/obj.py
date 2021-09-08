from placement import Placement
import struct
#FIXME use '!' (network order) instead of '<' in all pack functions
objects = dict()
nextObjUid = 0
def colorNetPack(c):
	return struct.pack('!BBBB', *[int(v*255) for v in c])
def removeAll():
	global objects, nextObjUid
	objects = dict()
def getUid():
	global objects, nextObjUid
	m = 2000000000
	nextObjUid = (nextObjUid+1)%m
	while nextObjUid in objects.keys():
		nextObjUid = (nextObjUid+1)%m
	return nextObjUid
class Obj:
	def __init__(self, collisionClass, color=(0.8, 0.8, 0.8, 1.0), name = ""):
		global objects
		self.collisionClass = collisionClass
		self.collisions = set()
		self.color = color
		self.uid = getUid()
		self.name = name
		self.revision = 0
		objects[self.uid] = self
	def setColor(self, c):
		self.color = c
		self.stepRevision()
	def setName(self, n):
		self.name = n
		self.stepRevision()
	def stepRevision(self):
		self.revision = (self.revision+1)%100;
	def remove(self):
		global objects
		objects.pop(self.uid)
class OctObj(Obj):
	def __init__(self, mid, collisionClass, color=(0.8, 0.8, 0.8, 1.0), solid=True, name = ""):
		super().__init__(collisionClass, color, name)
		self.pos = Placement()
		self.solid = solid
		self.mid = mid
	def netPack(self):
		return struct.pack('!ib', self.uid, self.revision) + self.pos.netPack()
	def netDef(self):
		return struct.pack('!bibi', ord('o'), self.uid, self.revision, self.mid)+colorNetPack(self.color)+(self.name+'\0').encode("UTF-8", errors="replace")
class LineObj(Obj):
	def __init__(self, collisionClass, color=(0.8, 0.8, 0.8, 1.0)):
		self.loc = (0,0,0)
		self.offset = (1,0,0)
		self.movement = 0
		self.solid = True
		super().__init__(collisionClass, color, "")
	def netPack(self):
		return struct.pack('!ii', self.uid, self.movement)
	def netDef(self):
		return struct.pack('!biiiiiii', ord('l'), self.uid, *(self.loc), *(self.offset))+colorNetPack(self.color)
