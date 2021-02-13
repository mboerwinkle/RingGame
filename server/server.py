import socket #for client connections
import select
import sys
import time
import json
import math
import random
import struct
import threading
import os
from collections import deque as deque
import obj

from placement import Placement
import waitFramerate
from hypercubeCollide import hypercol

framerate = 30.0
con_vel = 3.0/framerate #it takes a third of a second for binary input players to go from neutral to maximum control
collisionRules = [
[0, 1, 0, 0],#rings
[1, 1, 1, 1],#ships
[0, 1, 0, 1],#asteroids
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
	global cliInputs, collide
	#load model manifest
	readManifest()
	#start client listener
	startNetworking()
	#create collider object
	collide = hypercol.Hypercol(3)#request a 3D collider
	for x in manifest["models"]:
		collide.loadOClass(x['name'], "assets/"+x['name']+".nhc3")
	while True:
		print("Starting new round")
		setupNewRound()
		roundResults = roundLoop()
		print(roundResults)
		time.sleep(1)#inter-round delay
	collide = None


def readManifest():
	global manifest
	if not os.path.isdir("assets"):
		sys.exit("Fatal: ./assets directory does not exist!")
	fd = open("assets/manifest3.json", mode="r")
	if not fd:
		print("Could not open manifest!")
		return
	manifest = json.load(fd)
	#print(str(manifest))
	fd.close()

def setupNewRound():
	Client.dieAll()
	CommandPoint.removeAll()
	obj.removeAll()
	Team.clear()
	Team("CYAN", (0.0, 1.0, 0.941, 1.0))
	Team("MGTA", (1.0, 0.0, 0.941, 1.0))

	for c in random.sample(list(Client.clients), len(Client.clients)):
		c.team = Team.select()

	for idx in range(10):
		CommandPoint()
	##place them in a ring for reliable debugging
	#cpCount = len(CommandPoint.commandobjects)
	#for cpIdx in range(cpCount):
	#	angle = 2*math.pi*cpIdx/cpCount
	#	loc = [int(10000*math.sin(angle)), int(10000*math.cos(angle)), int(0)]
	#	cp = CommandPoint.commandobjects[list(CommandPoint.commandobjects)[cpIdx]]
	#	cp.setLocation(loc)
	##
	for idx in range(5):
		asteroid = obj.Obj(random.choice((3, 4, 6)), 2)
		asteroid.pos.randomize(20000)


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
			convcoord = ()
			for idx in range(3):
				convcoord = convcoord+(int(o.pos.loc[idx]/manifest['resolution']*2.0),)
			pt = collide.createPoint(convcoord)
			orient = collide.createOrientation(*o.pos.rot)
			oinst = collide.newOInstance(manifest['models'][o.mid]['name'], o.collisionClass, pt, orient, 1.0)
			collide.addInstance(oinst)
			bscollidedict[bscollideind] = o
			bscollideind+=1
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
	tok = i.msg.split()
	cli = clientsById[i.id]
	if tok[0] == 'CTRL':
		cli.control = int(tok[1], 16)
		#print(str(i.id)+" controls: "+str(cli.control))
	elif tok[0] == 'COMM':
		if len(tok) > 1 and tok[1][0] == '/':
			cli.runcommand(tok[1:])
		else:
			Client.chat(cli.name+": "+i.msg[5:200])#skip 'COMM ' prefix
	elif tok[0] == 'RDEF':
		uid = int(tok[1], 16);
		if uid in obj.objects.keys():
			targ = obj.objects[int(tok[1], 16)]
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
	def select():
		weakestTeam = Team.teams[0]
		for t in Team.teams:
			if t.members() < weakestTeam.members():
				weakestTeam = t
		return weakestTeam
	def clear():
		for x in Client.clients:
			x.team = None
		Team.teams = []
		print("Cleared Teams")
	def scoreboard():
		board = dict()
		for t in Team.teams:
			board[t.name] = t.points
		return board
	def netPack():
		return struct.pack('<iii', 2, Team.teams[0].points, Team.teams[1].points)
	def __init__(self, name, color):
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

class Missile:
	missiles = set()
	deadMissiles = set()
	def cleanDead():
		for dm in Missile.deadMissiles:
			dm.remove()
		Missile.deadMissiles = set()
	def __init__(self, originObj):
		self.obj = obj.Obj(5, 3)
		self.obj.pos.copy(originObj.pos)
		self.obj.pos.moveForward(0.6*(manifest['models'][self.obj.mid]['diameter']+manifest['models'][originObj.mid]['diameter']))
		self.lifetime = int(3*framerate)
		self.speed = manifest['models'][self.obj.mid]['speed']/framerate
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
		self.obj.pos.moveForward(self.speed)
		
class CommandPoint:
	commandobjects = dict()
	def __init__(self, objMid = 2, totemMid = 1, team=None):
		totemColor = (0.9, 0.9, 0.9, 1.0)
		self.team = team
		if team != None:
			totemColor = team.color
		#The obj is the capture box. The totem is an optional auxillary solid marker
		self.obj = obj.Obj(objMid, 0, color=(0.5, 0.75, 1.0, 0.5), solid=False)
		self.totem = obj.Obj(totemMid, 0, color=totemColor, solid=True)
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
		self.control = 0
		self.pitch = 0.0
		self.yaw = 0.0
		self.roll = 0.0
		self.throttle = 0.0
		self.dead = False
		self.name = "client_"+str(self.id)
		self.team = None
		self.team = Team.select()
		Client.clients.add(self)
		self.obj = None

	def runcommand(self, tok):
		print(self.name+' executing: '+' '.join(tok))
		for tidx in range(len(tok)):
			tok[tidx] = tok[tidx].upper()
		if tok[0] == '/NAME':
			self.name = tok[1]
			self.obj.setName(self.name)
			self.sendmsg('CONSName set: '+self.name)
		elif tok[0] == '/LIST':
			msg = "CONS"
			for c in Client.clients:
				msg += c.name+':'+c.team.name+","
			self.sendmsg(msg)
		elif tok[0] == '/HELP':
			self.sendmsg("CONS/NAME <yourname>\n/LIST")
		else:
			self.sendmsg("CONSUnknown command: "+tok[0]+"\nTry /HELP")

	def respawn(self, modelId):
		self.die()
		color = (0.7, 0.5, 0.5)
		if self.team != None:
			color = self.team.color
		self.obj = obj.Obj(0, 1, color = color, name = self.name)
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
		#print(self.buf.decode('UTF-8'))
		foundSomething = True
		while foundSomething:
			foundSomething = False
			for x in range(len(self.buf)):
				if self.buf[x] == ord('#'):
					foundSomething = True
					myinput = CliInput(self.id, self.buf[:x].decode("UTF-8"))
					self.buf = self.buf[x+1:]
					cliInputs.appendleft(myinput)
					#print("Got new client msg: "+str(myinput.msg)+"  Remaining buf: "+str(self.buf))
					break
	def sendAsg(self):
		if(self.obj):
			self.send(b"ASGN" + struct.pack('<i', self.obj.uid))
	def sendDef(self, o):
		self.send(b"ODEF" + o.netDef())
	def sendUpdate(self):
		fMsg = b"FRME" +Team.netPack()+ struct.pack('<i', len(obj.objects))
		for o in obj.objects.values():
			fMsg += o.netPack()
		self.send(fMsg)
	def sendmsg(self, msg):
		self.send(msg.encode("UTF-8"))
	def send(self, data):
		try:
			self.soc.sendall(len(data).to_bytes(4, byteorder='little')+data)
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
		turnspeed = maniModel['turn']/framerate
		posObj.rotZ(self.yaw*turnspeed)
		posObj.rotY(self.pitch*turnspeed)
		posObj.rotX(self.roll*turnspeed)

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
