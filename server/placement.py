import math
import random
import struct
def quatmult(a, b):
	ret = [0.0,0.0,0.0,0.0]
	ret[0]=(b[0] * a[0] - b[1] * a[1] - b[2] * a[2] - b[3] * a[3])
	ret[1]=(b[0] * a[1] + b[1] * a[0] + b[2] * a[3] - b[3] * a[2])
	ret[2]=(b[0] * a[2] - b[1] * a[3] + b[2] * a[0] + b[3] * a[1])
	ret[3]=(b[0] * a[3] + b[1] * a[2] - b[2] * a[1] + b[3] * a[0])
	return ret
def quatvecmult(v, q):
	rev = (q[0], -q[1], -q[2], -q[3])
	vq = (0, v[0], v[1], v[2])
	res = quatmult(q, quatmult(vq, rev))
	return [res[1], res[2], res[3]]
def normalize(v):
	l = 0
	for axis in v:
		l += axis**2
	l = math.sqrt(l)
	for idx in range(len(v)):
		v[idx] /= l
class Placement:#fixme, make forward and up functions that are cached on call until next change
	def __init__(self):
		self.loc = [0, 0, 0]
		self.rot = [1.0, 0.0, 0.0, 0.0]
		self.forward = [1, 0, 0]
		self.up = [0, 0, 1]
	def __str__(self):
		f = self.forward
		u = self.up
		return "Location: "+str(self.loc)+("\nForward Vector: [%.2f, %.2f, %.2f]" % (f[0], f[1], f[2]))+("\nUp Vector: [%.2f, %.2f, %.2f]" % (u[0], u[1], u[2]))
	def copy(self, other):
		self.loc = other.loc.copy()
		self.rot = other.rot.copy()
		self.forward = other.forward.copy()
		self.up = other.up.copy()
	def moveForward(self, amt):
		self.moveRel([self.forward[0]*amt, self.forward[1]*amt, self.forward[2]*amt])
		#print("Location is now "+str(self.loc))
	def randomize(self, bound):
		self.randomizeLocation(bound)
		self.randomizeOrientation()
	def randomizeOrientation(self):
		for idx in range(4):
			self.rot[idx] = random.uniform(-1,1)
		normalize(self.rot)
		self.setAuxVectors()
	def randomizeLocation(self, bound):
		for idx in range(3):
			self.loc[idx] = random.randint(-bound, bound)
	def moveRel(self, disp):
		self.loc[0] += int(disp[0])
		self.loc[1] += int(disp[1])
		self.loc[2] += int(disp[2])
	def moveAbs(self, loc):
		self.loc = loc.copy()
	def setAuxVectors(self):
		self.forward = quatvecmult([1, 0, 0], self.rot)
		self.up = quatvecmult([0, 0, 1], self.rot)
	def rotX(self, rad):
		self.rot = quatmult(self.rot, (math.cos(rad/2), math.sin(rad/2), 0, 0))
		self.setAuxVectors()
	def rotY(self, rad):
		self.rot = quatmult(self.rot, (math.cos(rad/2), 0, math.sin(rad/2), 0))
		self.setAuxVectors()
	def rotZ(self, rad):
		self.rot = quatmult(self.rot, (math.cos(rad/2), 0, 0, math.sin(rad/2)))
		self.setAuxVectors()
	def orientLike(self, other):
		self.rot = other.rot.copy()
		self.forward = other.forward.copy()
		self.up = other.up.copy()
	def netPack(self):
		l = self.loc
		r = self.rot
		return struct.pack('<iiiffff', l[0], l[1], l[2], r[0], r[1], r[2], r[3])
