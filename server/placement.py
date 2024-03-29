import math
import random
import struct
def normalize(v):
	l = 0
	for axis in v:
		l += axis**2
	l = math.sqrt(l)
	for idx in range(len(v)):
		v[idx] /= l
def magnitude(v):
	l = 0
	for axis in v:
		l += axis**2
	return math.sqrt(l)
class Quat(list):
	netPackMult = 32000.0
	def __init__(self, c = (1.0, 0.0, 0.0, 0.0)):
		self.forwardCache = None
		self.upCache = None
		super().__init__(c[:4])
	def __str__(self):
		f = self.forward()
		u = self.up()
		return ("Forward Vector: [%.2f, %.2f, %.2f]" % (f[0], f[1], f[2]))+("\nUp Vector: [%.2f, %.2f, %.2f]" % (u[0], u[1], u[2]))
	def clearCache(self):
		self.forwardCache = None
		self.upCache = None
	def forward(self):
		if not self.forwardCache:
			self.forwardCache = self.vecmult([1,0,0])
		return self.forwardCache
	def up(self):
		if not self.upCache:
			self.upCache = self.vecmult([0,0,1])
		return self.upCache
	def mult(self, b):
		a = self
		ret = [0.0,0.0,0.0,0.0]
		ret[0]=(b[0] * a[0] - b[1] * a[1] - b[2] * a[2] - b[3] * a[3])
		ret[1]=(b[0] * a[1] + b[1] * a[0] + b[2] * a[3] - b[3] * a[2])
		ret[2]=(b[0] * a[2] - b[1] * a[3] + b[2] * a[0] + b[3] * a[1])
		ret[3]=(b[0] * a[3] + b[1] * a[2] - b[2] * a[1] + b[3] * a[0])
		return Quat(ret)
	def dot(s, b):
		return s[0]*b[0]+s[1]*b[1]+s[2]*b[2]+s[3]*b[3]
	def vecmult(q, v):
		rev = Quat((q[0], -q[1], -q[2], -q[3]))
		vq = Quat((0, v[0], v[1], v[2]))
		res = q.mult(vq.mult(rev))
		return res[1:]
	def lerp(one, two, t):
		i = 1.0-t
		ret = Quat((two[0]*t+one[0]*i, two[1]*t+one[1]*i, two[2]*t+one[2]*i, two[3]*t+one[3]*i))
		normalize(ret)
		return ret
	def slerp(one, two, t = 1.0, maxangle = 3.142):
		#per wikipedia and shoemake slerp, one*( (one^-1) * two )^t
		maxangle *= 0.5
		#theta is the angle on the quaternion sphere. There are two revolutions per every revolution on the quaternion sphere
		dotv = one.dot(two)
		theta = math.acos(-1.0 if dotv < -1.0 else 1.0 if dotv > 1.0 else dotv)
		# Long paths can be prevented by negating one end if the dot product, cos Ω, is negative, thus ensuring that −90° ≤ Ω ≤ 90°. (WIKIPEDIA)
		negmult = 1
		if theta > math.pi/2:
			theta = -(theta-math.pi) # we need to do this to keep the rotation speed right
			negmult = -1
		if(t*theta > maxangle):
			t = maxangle/theta
		b = math.sin(theta)
		if b == 0:
			return Quat(c=two)
		m1 = math.sin((1-t)*theta)/b
		m2 = negmult * math.sin(t*theta)/b
		ret = Quat((one[0]*m1+two[0]*m2, one[1]*m1+two[1]*m2, one[2]*m1+two[2]*m2, one[3]*m1+two[3]*m2))
		normalize(ret)
		return ret
	def randomize(self):
		for idx in range(4):
			self[idx] = random.uniform(-1,1)
		#avoid extremely unlikely edge case
		if self[0] == 0.0:
				self[idx] == 0.001
		normalize(self)
		self.clearCache()
	def orientLike(self, other):
		for x in range(4):
			self[x] = other[x]
		self.clearCache()
	def rotX(self, rad):
		self.rotateBy(Quat((math.cos(rad/2), math.sin(rad/2), 0, 0)))
	def rotY(self, rad):
		self.rotateBy(Quat((math.cos(rad/2), 0, math.sin(rad/2), 0)))
	def rotZ(self, rad):
		self.rotateBy(Quat((math.cos(rad/2), 0, 0, math.sin(rad/2))))
	def rotateBy(self, q):
		self.orientLike(self.mult(q))
		self.clearCache()
	def copy(self):
		return Quat(self)
	def netpack(self):
		m = Quat.netPackMult
		return struct.pack('!hhhh', int(self[0]*m), int(self[1]*m), int(self[2]*m), int(self[3]*m))
	def netunpack(data):
		m = Quat.netPackMult
		raw = list(struct.unpack('!hhhh', data))
		for idx in range(4):
			raw[idx] /= m
		return Quat(raw)
class Placement:
	def __init__(self):
		self.loc = [0, 0, 0]
		self.rot = Quat()
	def __str__(self):
		return "Location: "+str(self.loc)+"\n"+str(self.rot)
	def copy(self, other):
		self.loc = other.loc.copy()
		self.rot = other.rot.copy()
	def moveBy(self, vec, amt):
		self.moveRel([vec[d]*amt for d in range(3)])
	def moveForward(self, amt):
		self.moveBy(self.rot.forward(), amt)
	def moveUp(self, amt):
		self.moveBy(self.rot.up(), amt)
	def randomize(self, bound):
		for idx in range(3):
			self.loc[idx] = random.randint(-bound, bound)
		self.rot.randomize()
	def moveRel(self, disp):
		self.loc[0] += int(disp[0])
		self.loc[1] += int(disp[1])
		self.loc[2] += int(disp[2])
	def moveAbs(self, loc):
		self.loc = loc.copy()
	def netPack(self):
		return struct.pack('!iii', *(self.loc))+self.rot.netpack()
