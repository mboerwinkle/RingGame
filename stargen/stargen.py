import random
import json
stars = []
bounds = [-2000000000, 2000000000]
starCount = 5000
generalAmt = 1.0 #amount of stars in complete random placement
for sIdx in range(starCount):
	star = [0,0,0]
	for dim in range(len(star)):
		star[dim] = random.randint(bounds[0], bounds[1])
	stars.append(star)
fp = open("stars.json", "w")
fp.write(json.dumps(stars, separators=(',',':')))
fp.close()
