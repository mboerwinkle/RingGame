from placement import Placement
import struct
class Obj:
	nextObjUid = 0;
	objects = []
	def __init__(self, mId, collisionClass, color=(0.8, 0.8, 0.8), solid=True):
		self.solid = solid
		self.collisionClass = collisionClass
		self.collisions = set()
		self.color = color
		self.mid = mId
		self.uid = Obj.nextObjUid
		self.name = bytearray(("\0" * 8).encode('ascii'))
		Obj.nextObjUid += 1
		self.pos = Placement()
		Obj.objects.append(self)
		#print("New obj "+str(self.uid))
	def setColor(self, c):
		self.color = c
	def netPack(self):
		return struct.pack('<i', self.mid) + self.name + struct.pack('<ffff', self.color[0], self.color[1], self.color[2], 1.0) + self.pos.netPack()
	def remove(self):
		Obj.objects.remove(self)
	def removeAll():
		Obj.objects = []
		Obj.nextObjUid = 0
