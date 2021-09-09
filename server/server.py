import socket #for client connections
import select
import sys
import time
import math
import random
import struct
import threading
import os
from collections import deque as deque
import obj
import placement
from placement import Placement
from placement import Quat
import waitFramerate
import assetManifest
from hypercubeCollide import hypercol

framerate = 30.0
con_vel = 3.0/framerate #it takes a third of a second for binary input players to go from neutral to maximum control
collisionRules = [
[0, 1, 0, 0],#rings
[1, 1, 1, 1],#ships
[0, 1, 0, 1],#asteroids
#[0, 1, 1, 1],
[0, 1, 1, 0]#missiles
]
cliInputs = deque()
collide = None
manifest = None

for ridx in range(len(collisionRules)):
	r = collisionRules[ridx]
	if len(r) != len(collisionRules):
		print("Improper collision rules! Invalid row lengths")
	for cidx in range(len(r)):
		if r[cidx] != collisionRules[cidx][ridx]:
			print("Improper collision rules! Not symmetric matrix")

class CliInput:
	def __init__(self, cliId, msg):
		self.id = cliId
		self.msg = msg

def init():
	global cliInputs, collide, manifest
	#load model manifest
	manifest = assetManifest.readManifest("assets/manifest3.json")
	if manifest == None:
		return
	#start client listener
	startNetworking()
	#create collider object
	collide = hypercol.Hypercol(3)#request a 3D collider
	for x in manifest["models"]:
		collide.loadOClass(x['name'], "assets/nhc3/"+x['name']+".nhc3")
	while True:
		print("Starting new round")
		setupNewRound()
		roundResults = roundLoop()
		print(roundResults)
		time.sleep(1)#inter-round delay
	collide = None

def setupNewRound():
	Client.dieAll()
	CommandPoint.removeAll()
	Missile.removeAll()
	obj.removeAll()
	Team.clear()
	Team("CYAN", (0.0, 1.0, 0.941, 1.0))
	Team("MGTA", (1.0, 0.0, 0.941, 1.0))

	for c in random.sample(list(Client.clients), len(Client.clients)):
		c.team = Team.select()

	for idx in range(10):
		CommandPoint()
	for idx in range(100):
		asteroid = obj.OctObj(random.choice((3, 4, 6)), 2)
		asteroid.pos.randomize(60000)
	#Team("Yellow", (1.0, 0.941, 0))

def roundLoop():
	global manifest, collide, framerate, clientLock
	start = time.monotonic()
	winner = None
	#test_iterations = 100
	test_iterations = -1
	while (not winner) and test_iterations != 0:
		if test_iterations > 0:
			test_iterations -= 1
		clientLock.release()
		if not test_iterations >= 0:
			waitFramerate.waitFramerate(1/framerate)
		clientLock.acquire()
		#Handle client inputs
		for hIdx in range(len(cliInputs)):
			handleClientInput(cliInputs.pop())
		#Find Collisions
		myscene = collide.newScene(collisionRules)
		collide.selectScene(myscene)
		bscollidedict = dict()#FIXME
		bscollideind = 0
		for o in obj.objects.values():
			if isinstance(o, obj.OctObj):
				convcoord = [int(o.pos.loc[idx]/manifest['resolution']*2.0) for idx in range(3)]
				pt = collide.createPoint(convcoord)
				orient = collide.createOrientation(*o.pos.rot)
				oinst = collide.newOInstance_o(o.collisionClass, pt, orient, manifest['models'][o.mid]['name'])
				collide.addInstance(oinst)
				bscollidedict[bscollideind] = o
				bscollideind+=1
			elif isinstance(o, obj.LineObj):
				vec = o.offset[:]
				placement.normalize(vec)
				start = collide.createPoint([int((o.loc[idx] + vec[idx] * o.movement)/manifest['resolution']*2.0) for idx in range(3)])
				offset = collide.createPoint([int(o.offset[idx]/manifest['resolution']*2.0) for idx in range(3)])
				oinst = collide.newOInstance_l(o.collisionClass, start, offset)
				collide.addInstance(oinst)
				bscollidedict[bscollideind] = o
				bscollideind+=1
			else:
				pass
				#assert isinstance(o, obj.SphereObj)
		collide.calculateScene()
		c = collide.getCollisions()
		collide.freeScene(myscene)
		for o in obj.objects.values():
			o.collisions.clear()
		#if c[0] != 0:
		#	print("Got",c[0],"collisions")
		for cIdx in range(c[0]):#first array element is the number of collisions
			o1 = bscollidedict[c[1+2*cIdx]]
			o2 = bscollidedict[c[2+2*cIdx]]
			o1.collisions.add(o2)
			o2.collisions.add(o1)
		#Step Physics
		commandObjKeys = CommandPoint.commandobjects.keys()
		for m in Missile.missiles:
			for col in m.obj.collisions:
				if col.solid:
					m.die()
					break
			m.step()
		for c in Client.clients:
			if c.obj == None:
				continue
			for col in c.obj.collisions:
				if col.solid:
					Client.chat(c.name+" died")
					c.respawn(0)
					if c.team != None:
						c.team.award(-1000)
				if col in commandObjKeys:
					CommandPoint.commandobjects[col].capture(c.team)
		for c in Client.clients:
			if c.obj == None:
				c.respawn(0)
			c.applyControls()
		Missile.cleanDead()
		#Add commandpoint score
		for t in Team.teams:
			t.award(len(t.commandPoints))
			if t.points > 10000:
				winner = t
				break
		#Send Client Data
		deadClients = set()
		for c in Client.clients:
			c.sendUpdate()
			if c.dead:
				deadClients.add(c)
		for dc in deadClients:
			print("Removing dead client")
			dc.remove()
	#sys.exit("exiting")
	#end = time.monotonic()
	#print(str(end-start))
	Client.chat("Team "+t.name+" Wins!")


def handleClientInput(i):
	comm = i.msg[0:4]
	rest = i.msg[4:]
	cli = clientsById[i.id]
	if comm == b'CTRL':
		cli.control = int(rest, 16)
		#print(str(i.id)+" controls: "+str(cli.control))
	elif comm == b'ORNT':
		cli.targetOrientation = Quat.netunpack(rest)
	elif comm == b'COMM':
		if len(rest) >= 1:
			if rest[0] == ord('/'):
				cli.runcommand(rest.decode('UTF-8').split())
			else:
				Client.chat(cli.name+": "+rest.decode('UTF-8'))
	elif comm == b'RDEF':
		uid = struct.unpack('!i', i.msg[4:])[0]
		if uid in obj.objects.keys():
			targ = obj.objects[uid]
			cli.sendDef(targ)
		else:
			print("ignoring request for", uid)
	else:
		print("Unknown message from "+str(i.id)+": "+str(i.msg))

#Client stuff
cliNetThread = None
def startNetworking():
	global cliNetThread, clientLock
	clientLock.acquire()
	cliSock = socket.socket(socket.AF_INET6, socket.SOCK_STREAM)
	cliSock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
	cliSock.bind(("", 3131))
	cliSock.listen()
	cliNetThread = threading.Thread(group=None, target=cliNetThreadLoop, name="cliNetThread", args=(cliSock,))
	cliNetThread.start()

class Team:
	teams = []
	netPackCache = None
	def select():
		if len(Team.teams) < 1:
			return None
		weakestTeam = Team.teams[0]
		for t in Team.teams:
			if t.members() < weakestTeam.members():
				weakestTeam = t
		return weakestTeam
	def clear():
		for x in Client.clients:
			x.team = None
		Team.teams = []
		Team.netPackCache = None
		print("Cleared Teams")
	def scoreboard():
		board = dict()
		for t in Team.teams:
			board[t.name] = t.points
		return board
	def getByName(n):
		for t in Team.teams:
			if t.name == n:
				return t
		return None
	def netPack():
		if not Team.netPackCache:
			Team.netPackCache = struct.pack('!b', len(Team.teams))
			for t in Team.teams:
				Team.netPackCache += struct.pack('!i', t.points)
		return Team.netPackCache
	def __init__(self, name, color):
		name = name.upper()
		#team names ending in underscore are individual user teams for ffa
		if Team.getByName(name):
			uniqueSuffix = 2
			while Team.getByName(name+"_"+str(uniqueSuffix)):
				uniqueSuffix += 1
			name = name+"_"+str(uniqueSuffix)
		self.name = name
		self.color = color
		self.points = 0
		self.commandPoints = set()
		Team.teams.append(self)
	def members(self):
		ret = 0
		for x in Client.clients:
			if x.team == self:
				ret+=1
		return ret

	def award(self, pts):
		self.points += pts
		Team.netPackCache = None

class Missile:
	missiles = set()
	deadMissiles = set()
	def cleanDead():
		for dm in Missile.deadMissiles:
			dm.remove()
		Missile.deadMissiles = set()
	def __init__(self, originObj):
		self.obj = obj.LineObj(3)
		tempplacement = Placement()
		tempplacement.copy(originObj.pos)
		tempplacement.moveForward(0.75*manifest['models'][originObj.mid]['diameter'])
		self.obj.loc = tempplacement.loc
		diameter = manifest['models'][5]['diameter']
		self.obj.offset = [int(x*diameter) for x in tempplacement.rot.forward()]
		self.lifetime = int(3*framerate)
		self.speed = int(manifest['models'][5]['speed']/framerate)
		Missile.missiles.add(self)
	def die(self):
		Missile.deadMissiles.add(self)
	def remove(self):
		Missile.missiles.remove(self)
		self.obj.remove()
	def step(self):
		self.lifetime -= 1
		if self.lifetime <= 0:
			self.die()
		self.obj.movement += self.speed
	def removeAll():
		Missile.missiles = set()
		Missile.deadMissiles = set()
class CommandPoint:
	commandobjects = dict()
	def __init__(self, objMid = 2, totemMid = 1, team=None):
		totemColor = (0.9, 0.9, 0.9, 1.0)
		self.team = team
		if team != None:
			totemColor = team.color
		#The obj is the capture box. The totem is an optional auxillary solid marker
		self.obj = obj.OctObj(objMid, 0, color=(0.5, 0.75, 1.0, 0.5), solid=False)
		self.totem = obj.OctObj(totemMid, 0, color=totemColor, solid=True)
		self.obj.pos.randomize(20000)
		self.totem.pos = self.obj.pos
		CommandPoint.commandobjects[self.obj] = self
	def capture(self, team):
		if self.team == team:
			return
		if self.team != None:
			self.team.commandPoints.remove(self)
		self.team = team
		if self.team != None:
			self.team.commandPoints.add(self)
		self.totem.setColor(team.color)
	def setLocation(self, p):
		self.obj.pos.moveAbs(p)
		self.totem.pos.moveAbs(p)
	def removeAll():
		CommandPoint.commandobjects = dict()

newCliId = 1
class Client:
	clients = set()
	def chat(msg):
		for c in Client.clients:
			c.sendmsg("CONS"+msg)

	def __init__(self, soc):
		global newCliId
		self.soc = soc
		self.id = newCliId
		newCliId += 1
		self.buf = bytes()
		self.obj = None
		self.control = 0
		self.pitch = 0.0
		self.yaw = 0.0
		self.roll = 0.0
		self.targetOrientation = Quat()
		self.throttle = 0.0
		self.dead = False
		self.name = "client_"+str(self.id)
		self.team = None
		self.setTeam(Team.select())
		Client.clients.add(self)


	def runcommand(self, tok):
		print(self.name+' executing: '+' '.join(tok))
		try:
			for tidx in range(len(tok)):
				tok[tidx] = tok[tidx].upper()
			if tok[0] == '/NAME':
				if(len(tok) < 2):
					self.sendmsg('CONSBad /NAME usage.\nTry /HELP')
				else:
					self.name = tok[1]
					self.obj.setName(self.name)
					self.sendmsg('CONSName set: '+self.name)
			elif tok[0] == '/LIST':
				msg = "CONS"
				for c in Client.clients:
					msg += c.name+':'+c.team.name+","
				self.sendmsg(msg)
			elif tok[0] == '/HELP':
				self.sendmsg("CONS/NAME <yourname>\n/LIST\n/TEAM [TEAMNAME]")
			elif tok[0] == '/TEAM':
				print("Token len:", len(tok))
				if(len(tok) < 2):
					if(self.team):
						self.sendmsg("CONS"+self.team.name)
				else:
					teamname = tok[1]
					myNewTeam = Team.getByName(teamname)
					if(myNewTeam):
						self.setTeam(myNewTeam)
						self.sendmsg('CONSTeam set: '+self.team.name)
					else:
						self.sendmsg('CONSTeam not found.')
			else:
				self.sendmsg("CONSUnknown command: "+tok[0]+"\nTry /HELP")
		except:
			print("Caught otherwise unhandled client command exception \"",tok,"\"")
	def respawn(self, modelId):
		self.die()
		color = (0.7, 0.5, 0.5)
		if self.team != None:
			color = self.team.color
		self.obj = obj.OctObj(0, 1, color = color, name = self.name)
		self.obj.pos.randomize(20000)
		self.sendAsg()
	def die(self):
		if self.obj == None:
			return
		o = self.obj
		self.obj = None
		o.remove()
	def dieAll():
		for c in Client.clients:
			c.die()
	def append(self, data):
		self.buf = self.buf+data
		foundSomething = True
		while foundSomething:
			foundSomething = False
			remainingLen = len(self.buf)
			if(remainingLen < 4):
				break
			nextMsgLen = struct.unpack('!i', self.buf[:4])[0]
			if remainingLen-4 >= nextMsgLen:
				foundSomething = True
				myinput = CliInput(self.id, self.buf[4:nextMsgLen+4])
				cliInputs.appendleft(myinput)
				self.buf = self.buf[nextMsgLen+4:]
	def setTeam(self, team):
		if(self.team == team):
			return
		self.team = team
		if(self.obj):
			self.obj.setColor(team.color)
		self.die()
	def sendAsg(self):
		if(self.obj):
			self.send(b"ASGN" + struct.pack('!ih', self.obj.uid, int(manifest['models'][self.obj.mid]['turn']*100.0)))
	def sendDef(self, o):
		self.send(b"ODEF" + o.netDef())
	def sendUpdate(self):
		fMsg = [b"FRME", Team.netPack()]
		octobj = list()
		lineobj = list()
		for o in obj.objects.values():
			if isinstance(o, obj.OctObj):
				octobj.append(o.netPack())
			else:
				assert isinstance(o, obj.LineObj)
				lineobj.append(o.netPack())
		fMsg.append(struct.pack('!i', len(octobj)))
		fMsg += octobj
		fMsg.append(struct.pack('!i', len(lineobj)))
		fMsg += lineobj
		self.send(b''.join(fMsg))
	def sendmsg(self, msg):
		self.send(msg.encode("UTF-8"))
	def send(self, data):
		try:
			self.soc.sendall(len(data).to_bytes(4, byteorder='big')+data)
		except Exception as e:
			print("Socket failed to write")
			self.ts_remove()
	def controlRange(targetValue, currentValue):
		delta = targetValue-currentValue
		return currentValue + max(-con_vel, min(con_vel, delta))
		#return targetValue
	def applyControls(self):
		global framerate
		if self.obj == None:
			return
		c = self.control
		self.yaw = Client.controlRange(((c>>1)&1)-((c>>2)&1), self.yaw)
		self.pitch = Client.controlRange(((c>>4)&1)-((c>>3)&1), self.pitch)
		self.roll = Client.controlRange(((c>>7)&1)-((c>>5)&1), self.roll)
		self.throttle = Client.controlRange(((c>>8)&1)-((c>>6)&1), self.throttle)
		#print(str(self.throttle)+' '+str(self.yaw)+' '+str(self.pitch)+' '+str(self.roll))
		posObj = self.obj.pos
		maniModel = manifest['models'][self.obj.mid]
		speed = maniModel['speed']/framerate
		trange = maniModel['trange']#throttle position range information
		if self.obj:
			self.obj.pos.rot = Quat.slerp(self.obj.pos.rot, self.targetOrientation, maxangle=manifest['models'][self.obj.mid]['turn']/framerate)
		realthrottle = self.throttle
		if self.throttle >= 0 :
			realthrottle = trange[1]+(self.throttle*(trange[2]-trange[1]))
		else:
			realthrottle = trange[1]-(self.throttle*(trange[0]-trange[1]))
		posObj.moveForward(realthrottle*speed)
		if c & 1:#Missile
			self.control &= ~1
			Missile(self.obj)
	def ts_remove(self):#thread-safe
		self.dead = True
	def remove(self):
		self.die()
		del clientsById[self.id]
		del clientsBySoc[self.soc]
		Client.clients.remove(self)
clientsBySoc = dict()
clientsById = dict()
selSocks = []
clientLock = threading.Lock()
def cliNetThreadLoop(soc):
	global clientsBySoc, clientsById, selSocks, clientLock
	selSocks.append(soc)
	while True:
		readable = select.select(selSocks,[],[])[0]
		for r in readable:
			if r is soc:
				newCliSoc = r.accept()[0]
				selSocks.append(newCliSoc)
				print("trying to acquire lock")
				clientLock.acquire()
				newCli = Client(newCliSoc)
				clientsBySoc[newCliSoc] = newCli
				clientsById[newCli.id] = newCli
				clientLock.release()
				print("New Client: "+str(newCli.id)+" ("+str(len(selSocks))+" total)")
			else:
				c = clientsBySoc[r]
				msg = b''
				try:
					msg = r.recv(4096);
				except Exception as e:
					print("Failed to read from client: "+str(c.id))
				if len(msg) == 0:
					print("Got Client disconnect: "+str(c.id))
					selSocks.remove(r)
					c.ts_remove()
				else:
					c.append(msg)
init()
