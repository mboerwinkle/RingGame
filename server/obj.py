from placement import Placement
import struct
#FIXME use '!' (network order) instead of '<' in all pack functions
objects = dict()
nextObjUid = 0
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
	def __init__(self, mId, collisionClass, color=(0.8, 0.8, 0.8), solid=True, name = ""):
		global objects
		self.solid = solid
		self.collisionClass = collisionClass
		self.collisions = set()
		self.color = color
		self.mid = mId
		self.uid = getUid()
		self.name = name
		self.predMode = 0 #prediction mode for clients.
		self.revision = 0
		self.pos = Placement()
		objects[self.uid] = self
	def setColor(self, c):
		self.color = c
		self.stepRevision()
	def setName(self, n):
		self.name = n
		self.stepRevision()
	def stepRevision(self):
		self.revision = (self.revision+1)%100;
	def netPack(self):
		return struct.pack('<ib', self.uid, self.revision) + self.pos.netPack()
	def netDef(self):
		return struct.pack('<ibibfff', self.uid, self.revision, self.mid, self.predMode, self.color[0], self.color[1], self.color[2])+(self.name+'\0').encode("UTF-8", errors="replace")
	def remove(self):
		global objects
		objects.pop(self.uid)

