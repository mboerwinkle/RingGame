import png
import sys
import json
if len(sys.argv) != 2:
	print("USAGE: process.py <png file>");
	exit()
reader = png.Reader(sys.argv[1])
data = reader.read()
width = data[0]
height = data[1]
rowdata = list(data[2])
for rowidx in range(len(rowdata)):
	rowdata[rowidx] = list(rowdata[rowidx])
	newdata = list()
	for colidx in range(len(rowdata[rowidx])):
		if colidx%4 == 0:
			newdata.append(rowdata[rowidx][colidx])
	rowdata[rowidx] = newdata

charWidth = int(width/94)
if charWidth*94 != width:
	print("Warning: image width not divisible by 94")
print("DIM: "+str(width)+"x"+str(height))
print("Character size: "+str(charWidth)+"x"+str(height))
outfilename = '.'.join(sys.argv[1].split('.')[:-1])+".json"
print("output: "+outfilename)
tricount = 0
def makerect(x1, y1, x2, y2):
	global tricount
	tricount += 2
	p1 = x1*(height+1)+y1
	p2 = x2*(height+1)+y1
	p3 = x1*(height+1)+y2
	p4 = x2*(height+1)+y2
	return list((p2,p1,p3,p2,p3,p4))
out = dict()
out["invaspect"] = height/charWidth
out["spacing"] = 1/charWidth #one pixel
letterPoints = list()
letterTris = list()
letterStarts = list()
out["points"] = letterPoints
out["form"] = letterTris
out["letter"] = letterStarts
for x in range(charWidth+1):
	for y in range(height+1):
		letterPoints += list((x/charWidth, y/height))

for idx in range(94):
	res = list()
	for col in range(charWidth):
		start = None
		for row in range(height):
			pix = rowdata[row][col+idx*charWidth]
			if pix == 0:#it exists
				if start == None:
					start = row
			else:#it does not exist
				if start != None:
					res += makerect(col,start,(col+1),row)
					start = None
		if(start != None):
			res += makerect(col,start,(col+1),height)
			start = None
	letterStarts.append(len(letterTris))
	letterTris += res
with open(outfilename, "w") as fp:
	json.dump(out, fp, separators=(',',':'))
print("triangle count: "+str(tricount))
#def conv(a):
#	return ("#" if a == 255 else " ")
#for row in rowdata:
#	print("".join(list(map(conv, row))))
